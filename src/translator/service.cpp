#include "service.h"
#include "definitions.h"

#include <string>
#include <utility>

namespace marian {
namespace bergamot {

Service::Service(Ptr<Options> options)
    : requestId_(0), batchNumber_(0),
      numWorkers_(options->get<int>("cpu-threads")),
      vocabs_(std::move(loadVocabularies(options))),
      text_processor_(vocabs_, options), batcher_(options),
      pcqueue_(2 * options->get<int>("cpu-threads")) {

  workers_.reserve(numWorkers_);

  for (int cpuId = 0; cpuId < numWorkers_; cpuId++) {
    workers_.emplace_back([&] {
      marian::DeviceId deviceId(cpuId, DeviceType::cpu);
      translation_loop(deviceId, pcqueue_, vocabs_, options);
    });
  }
}

std::future<TranslationResult> Service::translateWithCopy(std::string input) {
  return translate(std::move(input));
}

std::future<TranslationResult> Service::translate(std::string &&input) {
  // Takes in a blob of text. Segments and std::vector<TokenRanges> are
  // extracted from the input (blob of text) and used to construct a Request
  // along with a promise. promise value is set by the worker completing a
  // request.
  //
  // Batcher, which currently runs on the main thread constructs batches out of
  // a single request (at the moment) and adds them into a Producer-Consumer
  // queue holding a bunch of requestSentences used to construct batches.
  // TODO(jerin): Make asynchronous and compile from multiple requests.
  //
  // returns future corresponding to the promise.

  Segments segments;
  std::vector<TokenRanges> sourceAlignments;
  text_processor_.process(input, segments, sourceAlignments);

  std::promise<TranslationResult> translationResultPromise;
  auto future = translationResultPromise.get_future();

  Ptr<Request> request = New<Request>(
      requestId_++, /* lineNumberBegin = */ 0, vocabs_, std::move(input),
      std::move(segments), std::move(sourceAlignments),
      std::move(translationResultPromise));

  for (int i = 0; i < request->numSegments(); i++) {
    RequestSentence requestSentence(i, request);
    batcher_.addSentenceWithPriority(requestSentence);
  }

  int numSentences;
  do {
    RequestSentences batchSentences;
    batcher_.cleaveBatch(batchSentences);
    numSentences = batchSentences.size();

    if (numSentences > 0) {
      PCItem pcitem(batchNumber_++, std::move(batchSentences));
      pcqueue_.ProduceSwap(pcitem);
    }

    if (batchNumber_ % 500 == 0) {
      LOG(info, "Queuing batch {}", batchNumber_);
    }
  } while (numSentences > 0);

  return future;
}

void Service::stop() {
  int counter = 0;
  for (auto &worker : workers_) {
    PCItem pcitem;
    pcqueue_.ProduceSwap(pcitem);
    ++counter;
  }

  counter = 0;
  for (auto &worker : workers_) {
    worker.join();
    ++counter;
  }

  workers_.clear(); // Takes care of idempotency.
}

Service::~Service() { stop(); }

// Internal function nobody used, only within service.
std::vector<Ptr<const Vocab>> loadVocabularies(Ptr<Options> options) {
  // @TODO: parallelize vocab loading for faster startup
  auto vfiles = options->get<std::vector<std::string>>("vocabs");
  // with the current setup, we need at least two vocabs: src and trg
  ABORT_IF(vfiles.size() < 2, "Insufficient number of vocabularies.");
  std::vector<Ptr<Vocab const>> vocabs(vfiles.size());
  std::unordered_map<std::string, Ptr<Vocab>> vmap;
  for (size_t i = 0; i < vocabs.size(); ++i) {
    auto m = vmap.emplace(std::make_pair(vfiles[i], Ptr<Vocab>()));
    if (m.second) { // new: load the vocab
      m.first->second = New<Vocab>(options, i);
      m.first->second->load(vfiles[i]);
    }
    vocabs[i] = m.first->second;
  }
  return vocabs;
}

} // namespace bergamot
} // namespace marian
