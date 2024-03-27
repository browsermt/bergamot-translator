#include "translation_model.h"

#include "batch.h"
#include "byte_array_util.h"
#include "cache.h"
#include "common/logging.h"
#include "data/corpus.h"
#include "data/text_input.h"
#include "html.h"
#include "parser.h"
#include "translator/beam_search.h"

namespace marian {
namespace bergamot {

std::atomic<size_t> TranslationModel::modelCounter_ = 0;

TranslationModel::TranslationModel(const Config &options, MemoryBundle &&memory /*=MemoryBundle{}*/,
                                   size_t replicas /*=1*/, std::vector<size_t> gpus /*={}*/)
    : modelId_(modelCounter_++),
      options_(options),
      memory_(std::move(memory)),
      vocabs_(options, std::move(memory_.vocabs)),
      textProcessor_(options, vocabs_, std::move(memory_.ssplitPrefixFile)),
      batchingPool_(options),
      gpus_{gpus},
      qualityEstimator_(createQualityEstimator(getQualityEstimatorModel(memory, options))) {
  ABORT_IF(replicas == 0, "At least one replica needs to be created.");
  backend_.resize(replicas);

  // Try to load shortlist from memory-bundle. If not available, try to load from options_;

  int srcIdx = 0, trgIdx = 1;
  // vocabs_->sources().front() is invoked as we currently only support one source vocab
  bool shared_vcb = (vocabs_.sources().front() == vocabs_.target());

  if (memory_.shortlist.size() > 0 && memory_.shortlist.begin() != nullptr) {
    bool check = options_->get<bool>("check-bytearray", false);
    shortlistGenerator_ = New<data::BinaryShortlistGenerator>(memory_.shortlist.begin(), memory_.shortlist.size(),
                                                              vocabs_.sources().front(), vocabs_.target(), srcIdx,
                                                              trgIdx, shared_vcb, check);
  } else if (options_->hasAndNotEmpty("shortlist")) {
    // Changed to BinaryShortlistGenerator to enable loading binary shortlist file
    // This class also supports text shortlist file
    shortlistGenerator_ = New<data::BinaryShortlistGenerator>(options_, vocabs_.sources().front(), vocabs_.target(),
                                                              srcIdx, trgIdx, shared_vcb);
  } else {
    // In this case, the loadpath does not load shortlist.
    shortlistGenerator_ = nullptr;
  }
}

void TranslationModel::loadBackend(size_t idx) {
  auto &graph = backend_[idx].graph;
  auto &scorerEnsemble = backend_[idx].scorerEnsemble;

  marian::DeviceId device_;

  if (gpus_.empty()) {
    device_ = marian::DeviceId(idx, DeviceType::cpu);
  } else {
    device_ = marian::DeviceId(gpus_[idx], DeviceType::gpu);
  }
  graph = New<ExpressionGraph>(/*inference=*/true);  // set the graph to be inference only
  auto prec = options_->get<std::vector<std::string>>("precision", {"float32"});
  graph->setDefaultElementType(typeFromString(prec[0]));
  graph->setDevice(device_);
  graph->getBackend()->configureDevice(options_);
  graph->reserveWorkspaceMB(options_->get<size_t>("workspace"));

  // if memory_.models is populated, then all models were of binary format
  if (memory_.models.size() >= 1) {
    const std::vector<const void *> container = std::invoke([&]() {
      std::vector<const void *> model_ptrs(memory_.models.size());
      for (size_t i = 0; i < memory_.models.size(); ++i) {
        const AlignedMemory &model = memory_.models[i];

        ABORT_IF(model.size() == 0 || model.begin() == nullptr, "The provided memory is empty. Cannot load the model.");
        ABORT_IF(
            (uintptr_t)model.begin() % 256 != 0,
            "The provided memory is not aligned to 256 bytes and will crash when vector instructions are used on it.");
        if (options_->get<bool>("check-bytearray", false)) {
          ABORT_IF(!validateBinaryModel(model, model.size()),
                   "The binary file is invalid. Incomplete or corrupted download?");
        }

        model_ptrs[i] = model.begin();
        LOG(debug, "Loaded model {} of {} from memory", (i + 1), model_ptrs.size());
      }
      return model_ptrs;
    });

    scorerEnsemble = createScorers(options_, container);
  } else {
    // load npz format models, or a mixture of binary/npz formats
    scorerEnsemble = createScorers(options_);
    LOG(debug, "Loaded {} model(s) from file", scorerEnsemble.size());
  }

  for (auto scorer : scorerEnsemble) {
    scorer->init(graph);
    if (shortlistGenerator_) {
      scorer->setShortlistGenerator(shortlistGenerator_);
    }
  }
  graph->forward();
}

// Make request process is shared between Async and Blocking workflow of translating.
Ptr<Request> TranslationModel::makeRequest(size_t requestId, std::string &&source, CallbackType callback,
                                           const ResponseOptions &responseOptions,
                                           std::optional<TranslationCache> &cache) {
  Segments segments;
  AnnotatedText annotatedSource;

  textProcessor_.process(std::move(source), annotatedSource, segments);
  ResponseBuilder responseBuilder(responseOptions, std::move(annotatedSource), vocabs_, callback, *qualityEstimator_);

  Ptr<Request> request =
      New<Request>(requestId, /*model=*/*this, std::move(segments), std::move(responseBuilder), cache);
  return request;
}

Ptr<Request> TranslationModel::makePivotRequest(size_t requestId, AnnotatedText &&previousTarget, CallbackType callback,
                                                const ResponseOptions &responseOptions,
                                                std::optional<TranslationCache> &cache) {
  Segments segments;

  textProcessor_.processFromAnnotation(previousTarget, segments);
  ResponseBuilder responseBuilder(responseOptions, std::move(previousTarget), vocabs_, callback, *qualityEstimator_);

  Ptr<Request> request = New<Request>(requestId, *this, std::move(segments), std::move(responseBuilder), cache);
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

void TranslationModel::translateBatch(size_t deviceId, Batch &batch) {
  auto &backend = backend_[deviceId];

  if (!backend.initialized) {
    loadBackend(deviceId);
    backend.initialized = true;
  }

  BeamSearch search(options_, backend.scorerEnsemble, vocabs_.target());
  Histories histories = search.search(backend.graph, convertToMarianBatch(batch));
  batch.completeBatch(histories);
}

}  // namespace bergamot
}  // namespace marian
