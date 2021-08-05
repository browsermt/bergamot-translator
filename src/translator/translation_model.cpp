#include "translation_model.h"

#include "batch.h"
#include "byte_array_util.h"
#include "common/logging.h"
#include "data/corpus.h"
#include "data/text_input.h"
#include "parser.h"
#include "translator/beam_search.h"

namespace marian {
namespace bergamot {

TranslationModel::TranslationModel(const std::string &config, MemoryBundle &&memory)
    : TranslationModel(parseOptions(config, /*validate=*/false), std::move(memory), /*replicas=*/1){};

TranslationModel::TranslationModel(Ptr<Options> options, MemoryBundle &&memory, size_t replicas)
    : options_(options),
      memory_(std::move(memory)),
      vocabs_(options, std::move(memory_.vocabs)),
      textProcessor_(options, vocabs_, std::move(memory_.ssplitPrefixFile)),
      batchingPool_(options) {
  ABORT_IF(replicas == 0, "At least one replica needs to be created.");
  backend_.resize(replicas);
  // ShortList: Load from memoryBundle or options
  for (size_t idx = 0; idx < replicas; idx++) {
    loadBackend(idx);
  }
}

void TranslationModel::loadBackend(size_t idx) {
  // Aliasing to reuse old code.
  auto &slgen_ = backend_[idx].shortlistGenerator;
  auto &graph_ = backend_[idx].graph;
  auto &scorers_ = backend_[idx].scorerEnsemble;

  marian::DeviceId device_(idx, DeviceType::cpu);

  // Old code.
  if (options_->hasAndNotEmpty("shortlist")) {
    int srcIdx = 0, trgIdx = 1;
    bool shared_vcb =
        vocabs_.sources().front() ==
        vocabs_.target();  // vocabs_->sources().front() is invoked as we currently only support one source vocab
    if (memory_.shortlist.size() > 0 && memory_.shortlist.begin() != nullptr) {
      slgen_ = New<data::BinaryShortlistGenerator>(memory_.shortlist.begin(), memory_.shortlist.size(),
                                                   vocabs_.sources().front(), vocabs_.target(), srcIdx, trgIdx,
                                                   shared_vcb, options_->get<bool>("check-bytearray"));
    } else {
      // Changed to BinaryShortlistGenerator to enable loading binary shortlist file
      // This class also supports text shortlist file
      slgen_ = New<data::BinaryShortlistGenerator>(options_, vocabs_.sources().front(), vocabs_.target(), srcIdx,
                                                   trgIdx, shared_vcb);
    }
  }

  graph_ = New<ExpressionGraph>(true);  // set the graph to be inference only
  auto prec = options_->get<std::vector<std::string>>("precision", {"float32"});
  graph_->setDefaultElementType(typeFromString(prec[0]));
  graph_->setDevice(device_);
  graph_->getBackend()->configureDevice(options_);
  graph_->reserveWorkspaceMB(options_->get<size_t>("workspace"));

  // Marian Model: Load from memoryBundle or shortList
  if (memory_.model.size() > 0 &&
      memory_.model.begin() !=
          nullptr) {  // If we have provided a byte array that contains the model memory, we can initialise the
                      // model from there, as opposed to from reading in the config file
    ABORT_IF((uintptr_t)memory_.model.begin() % 256 != 0,
             "The provided memory is not aligned to 256 bytes and will crash when vector instructions are used on it.");
    if (options_->get<bool>("check-bytearray")) {
      ABORT_IF(!validateBinaryModel(memory_.model, memory_.model.size()),
               "The binary file is invalid. Incomplete or corrupted download?");
    }
    const std::vector<const void *> container = {
        memory_.model.begin()};  // Marian supports multiple models initialised in this manner hence std::vector.
                                 // However we will only ever use 1 during decoding.
    scorers_ = createScorers(options_, container);
  } else {
    scorers_ = createScorers(options_);
  }
  for (auto scorer : scorers_) {
    scorer->init(graph_);
    if (slgen_) {
      scorer->setShortlistGenerator(slgen_);
    }
  }
  graph_->forward();
}

void translateBatch(size_t deviceId, Ptr<TranslationModel> model, Batch &batch) {
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

  size_t batchSize = batchVector.size();
  std::vector<size_t> sentenceIds;
  std::vector<int> maxDims;
  for (auto &ex : batchVector) {
    if (maxDims.size() < ex.size()) maxDims.resize(ex.size(), 0);
    for (size_t i = 0; i < ex.size(); ++i) {
      if (ex[i].size() > (size_t)maxDims[i]) maxDims[i] = (int)ex[i].size();
    }
    sentenceIds.push_back(ex.getId());
  }

  typedef marian::data::SubBatch SubBatch;
  typedef marian::data::CorpusBatch CorpusBatch;

  std::vector<Ptr<SubBatch>> subBatches;
  for (size_t j = 0; j < maxDims.size(); ++j) {
    subBatches.emplace_back(New<SubBatch>(batchSize, maxDims[j], model->vocabs().sources().at(j)));
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

  for (size_t j = 0; j < maxDims.size(); ++j) subBatches[j]->setWords(words[j]);

  auto corpus_batch = New<CorpusBatch>(subBatches);
  corpus_batch->setSentenceIds(sentenceIds);

  auto search = New<BeamSearch>(model->options(), model->scorerEnsemble(deviceId), model->vocabs().target());
  auto histories = search->search(model->graph(deviceId), corpus_batch);
  batch.completeBatch(histories);
}

}  // namespace bergamot
}  // namespace marian
