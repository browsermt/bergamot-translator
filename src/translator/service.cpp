#include "service.h"
#include "batch.h"
#include "definitions.h"

#include <string>
#include <utility>

namespace marian {
namespace bergamot {

Service::Service(Ptr<Options> options)
    : requestId_(0), numWorkers_(options->get<int>("cpu-threads")),
      vocabs_(std::move(loadVocabularies(options))),
      text_processor_(vocabs_, options), batcher_(options),
      pcqueue_(2 * options->get<int>("cpu-threads")) {

  if (numWorkers_ == 0) {
    // In case workers are 0, a single-translator is created and initialized
    // in the main thread.
    marian::DeviceId deviceId(/*cpuId=*/0, DeviceType::cpu);
    translators_.emplace_back(deviceId, vocabs_, options);
    translators_.back().initialize();
  } else {
    // If workers specified are greater than 0, translators_ are populated with
    // unitialized instances. These are then initialized inside
    // individual threads and set to consume from producer-consumer queue.
    workers_.reserve(numWorkers_);
    translators_.reserve(numWorkers_);
    for (size_t cpuId = 0; cpuId < numWorkers_; cpuId++) {
      marian::DeviceId deviceId(cpuId, DeviceType::cpu);
      translators_.emplace_back(deviceId, vocabs_, options);

      auto &translator = translators_.back();
      workers_.emplace_back([&translator, this] {
        translator.initialize();
        translator.consumeFrom(pcqueue_);
      });
    }
  }
}

std::future<Response> Service::translateWithCopy(std::string input) {
  return translate(std::move(input));
}

std::future<Response> Service::translate(std::string &&input) {
  // Takes in a blob of text. Segments and SentenceRanges are
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
  SentenceRanges sourceRanges;
  text_processor_.process(input, segments, sourceRanges);

  std::promise<Response> responsePromise;
  auto future = responsePromise.get_future();

  Ptr<Request> request = New<Request>(
      requestId_++, /* lineNumberBegin = */ 0, vocabs_, std::move(input),
      std::move(segments), std::move(sourceRanges), std::move(responsePromise));

  batcher_.addWholeRequest(request);

  if (numWorkers_ > 0) {
    batcher_.produceTo(pcqueue_);
  } else {
    // Queue single-threaded
    Batch batch;
    while (batcher_ >> batch) {
      translators_[0].translate(batch);
    }
  }

  return future;
}

void Service::stop() {
  for (auto &worker : workers_) {
    Batch poison = Batch::poison();
    pcqueue_.ProduceSwap(poison);
  }

  for (auto &worker : workers_) {
    worker.join();
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
