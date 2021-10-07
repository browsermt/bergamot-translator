#include "translation_model.h"

#include "batch.h"
#include "byte_array_util.h"
#include "common/logging.h"
#include "data/corpus.h"
#include "data/text_input.h"
#include "parser.h"
#include "service.h"
#include "translator/beam_search.h"

namespace marian {
namespace bergamot {

TranslationModel::TranslationModel(const Config &options, MemoryBundle &&memory /*=MemoryBundle{}*/,
                                   size_t replicas /*=1*/)
    : options_(options),
      memory_(std::move(memory)),
      vocabs_(options, std::move(memory_.vocabs)),
      textProcessor_(options, vocabs_, std::move(memory_.ssplitPrefixFile)),
      batchingPool_(options),
      qualityEstimator_(createQualityEstimator(getQualityEstimatorModel(memory, options))) {
  // ABORT_IF(replicas == 0, "At least one replica needs to be created.");
  // backend_.resize(replicas);

  if (options_->hasAndNotEmpty("shortlist")) {
    int srcIdx = 0, trgIdx = 1;
    bool shared_vcb =
        vocabs_.sources().front() ==
        vocabs_.target();  // vocabs_->sources().front() is invoked as we currently only support one source vocab
    if (memory_.shortlist.size() > 0 && memory_.shortlist.begin() != nullptr) {
      bool check = options_->get<bool>("check-bytearray", false);
      shortlistGenerator_ = New<data::BinaryShortlistGenerator>(memory_.shortlist.begin(), memory_.shortlist.size(),
                                                                vocabs_.sources().front(), vocabs_.target(), srcIdx,
                                                                trgIdx, shared_vcb, check);
    } else {
      // Changed to BinaryShortlistGenerator to enable loading binary shortlist file
      // This class also supports text shortlist file
      shortlistGenerator_ = New<data::BinaryShortlistGenerator>(options_, vocabs_.sources().front(), vocabs_.target(),
                                                                srcIdx, trgIdx, shared_vcb);
    }
  }

  /*
  for (size_t idx = 0; idx < replicas; idx++) {
    loadBackend(idx);
  }
  */
}

void TranslationModel::loadBackend(MarianBackend &backend, Workspace &workspace) {
  auto &graph = backend.graph;
  auto &scorerEnsemble = backend.scorerEnsemble;

  graph = New<ExpressionGraph>(/*inference=*/true);  // set the graph to be inference only
  graph->setDefaultElementType(workspace.precision());
  graph->setDevice(workspace.device());

  // This graph is for certain required to be configured to the TranslationModel's options.
  graph->getBackend()->configureDevice(options_);

  // The translation-model's graph share workspace bound to threads, with other translation-models.
  graph->setWorkspaces(workspace.tensors(), workspace.cache());

  // Marian Model: Load from memoryBundle or shortList
  if (memory_.model.size() > 0 &&
      memory_.model.begin() !=
          nullptr) {  // If we have provided a byte array that contains the model memory, we can initialise the
                      // model from there, as opposed to from reading in the config file
    ABORT_IF((uintptr_t)memory_.model.begin() % 256 != 0,
             "The provided memory is not aligned to 256 bytes and will crash when vector instructions are used on it.");
    if (options_->get<bool>("check-bytearray", false)) {
      ABORT_IF(!validateBinaryModel(memory_.model, memory_.model.size()),
               "The binary file is invalid. Incomplete or corrupted download?");
    }
    const std::vector<const void *> container = {
        memory_.model.begin()};  // Marian supports multiple models initialised in this manner hence std::vector.
                                 // However we will only ever use 1 during decoding.
    scorerEnsemble = createScorers(options_, container);
  } else {
    scorerEnsemble = createScorers(options_);
  }
  for (auto scorer : scorerEnsemble) {
    scorer->init(graph);
    if (shortlistGenerator_) {
      scorer->setShortlistGenerator(shortlistGenerator_);
    }
  }

  // Forward consumes nodeForward and we'll be unable to do anything.
  auto deviceId = graph->getDeviceId().no;
  if (deviceId == 0) {
    std::cout << graph->graphviz() << std::endl;
    graph->pprintTensors();
  }

  graph->forward();
}

// Make request process is shared between Async and Blocking workflow of translating.
Ptr<Request> TranslationModel::makeRequest(size_t requestId, std::string &&source, CallbackType callback,
                                           const ResponseOptions &responseOptions) {
  Segments segments;
  AnnotatedText annotatedSource;

  textProcessor_.process(std::move(source), annotatedSource, segments);
  ResponseBuilder responseBuilder(responseOptions, std::move(annotatedSource), vocabs_, callback, *qualityEstimator_);

  Ptr<Request> request = New<Request>(requestId, std::move(segments), std::move(responseBuilder));
  return request;
}

Ptr<marian::data::CorpusBatch> TranslationModel::convertToMarianBatch(Batch &batch) {
  std::vector<data::SentenceTuple> batchVector;
  auto &sentences = batch.sentences();

  size_t batchSequenceNumber{0};
  for (auto &sentence : sentences) {
    data::SentenceTuple sentence_tuple(batchSequenceNumber);
    Segment segment = sentence.getUnderlyingSegment();
    sentence_tuple.push_back(segment);
    batchVector.push_back(sentence_tuple);

    ++batchSequenceNumber;
  }

  // Usually one would expect inputs to be [B x T], where B = batch-size and T = max seq-len among the sentences in the
  // batch. However, marian's library supports multi-source and ensembling through different source-vocabulary but same
  // target vocabulary. This means the inputs are 3 dimensional when converted into marian's library formatted batches.
  //
  // Consequently B x T projects to N x B x T, where N = ensemble size. This adaptation does not fully force the idea of
  // N = 1 (the code remains general, but N iterates only from 0-1 in the nested loop).

  size_t batchSize = batchVector.size();

  std::vector<size_t> sentenceIds;
  std::vector<int> maxDims;

  for (auto &example : batchVector) {
    if (maxDims.size() < example.size()) {
      maxDims.resize(example.size(), 0);
    }
    for (size_t i = 0; i < example.size(); ++i) {
      if (example[i].size() > static_cast<size_t>(maxDims[i])) {
        maxDims[i] = static_cast<int>(example[i].size());
      }
    }
    sentenceIds.push_back(example.getId());
  }

  using SubBatch = marian::data::SubBatch;
  std::vector<Ptr<SubBatch>> subBatches;
  for (size_t j = 0; j < maxDims.size(); ++j) {
    subBatches.emplace_back(New<SubBatch>(batchSize, maxDims[j], vocabs_.sources().at(j)));
  }

  std::vector<size_t> words(maxDims.size(), 0);
  for (size_t i = 0; i < batchSize; ++i) {
    for (size_t j = 0; j < maxDims.size(); ++j) {
      for (size_t k = 0; k < batchVector[i][j].size(); ++k) {
        subBatches[j]->data()[k * batchSize + i] = batchVector[i][j][k];
        subBatches[j]->mask()[k * batchSize + i] = 1.f;
        words[j]++;
      }
    }
  }

  for (size_t j = 0; j < maxDims.size(); ++j) {
    subBatches[j]->setWords(words[j]);
  }

  using CorpusBatch = marian::data::CorpusBatch;
  Ptr<CorpusBatch> corpusBatch = New<CorpusBatch>(subBatches);
  corpusBatch->setSentenceIds(sentenceIds);
  return corpusBatch;
}

void TranslationModel::translateBatch(Workspace &workspace, Batch &batch) {
  // We're the only people accessing this workspace, it's safe to clear.
  // Expectation is that the workspace contains things that don't require long-term storage (per batch things).

  // Parameters are stored separately and hopefully initialized and kept-isolated in the graph after
  // scorer->init(graph) in loadBackend(...).

  // This allows to avoid any leaks and generate maximum room for this incoming translation on the workspace.
  workspace.clear();

  // Create backend if not exists, for device. Dynamically.
  size_t deviceId = workspace.id();
  auto p = backend_.find(deviceId);
  if (p == backend_.end()) {
    backend_[deviceId] = MarianBackend{};
    loadBackend(backend_[deviceId], workspace);
  }

  auto &backend = backend_[deviceId];

  BeamSearch search(options_, backend.scorerEnsemble, vocabs_.target());
  Histories histories = search.search(backend.graph, convertToMarianBatch(batch));
  batch.completeBatch(histories);
  (backend.graph)->logActive();
}

}  // namespace bergamot
}  // namespace marian
