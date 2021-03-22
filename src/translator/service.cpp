#include "service.h"
#include "batch.h"
#include "definitions.h"

#include <string>
#include <utility>

namespace marian {
namespace bergamot {

Service::Service(Ptr<Options> options, const void *model_memory)
    : requestId_(0), vocabs_(std::move(loadVocabularies(options))),
      text_processor_(vocabs_, options), batcher_(options),
      numWorkers_(options->get<int>("cpu-threads")),
      pcqueue_(numWorkers_), model_memory_{model_memory} {

  if (numWorkers_ == 0) {
    initialize_blocking_translator(options);
  } else {
    initialize_async_translators(options);
  }
}

void Service::initialize_blocking_translator(Ptr<Options> options) {
  DeviceId deviceId(0, DeviceType::cpu);
  translators_.emplace_back(deviceId, vocabs_, options, model_memory_);
  translators_.back().initialize();
}

void Service::blocking_translate() {
  Batch batch;
  while (batcher_ >> batch) {
    auto &translator = translators_.back();
    translator.translate(batch);
  }
}

#ifdef WITH_PTHREADS
void Service::initialize_async_translators(Ptr<Options> options) {
  translators_.reserve(numWorkers_);
  workers_.reserve(numWorkers_);

  for (size_t cpuId = 0; cpuId < numWorkers_; cpuId++) {
    marian::DeviceId deviceId(cpuId, DeviceType::cpu);
    translators_.emplace_back(deviceId, vocabs_, options, model_memory_);
    auto &translator = translators_.back();

    workers_.emplace_back([&translator, this] {
      translator.initialize();

      // Run thread mainloop
      Batch batch;
      Histories histories;
      while (true) {
        pcqueue_.ConsumeSwap(batch);
        if (batch.isPoison()) {
          return;
        } else {
          translator.translate(batch);
        }
      }
    });
  }
}

void Service::async_translate() {
  Batch batch;
  while (batcher_ >> batch) {
    pcqueue_.ProduceSwap(batch);
  }
}
#else  // WITH_PTHREADS
void Service::initialize_async_translators(Ptr<Options> options) {
  ABORT("Cannot run in async mode without multithreading.")
}
void Service::async_translate(Ptr<Options> options) {
  ABORT("Cannot run in async mode without multithreading.")
}
#endif // WITH_PTHREADS

std::future<Response> Service::translate(std::string &&input) {
  Segments segments;
  SentenceRanges sourceRanges;
  text_processor_.process(input, segments, sourceRanges);

  std::promise<Response> responsePromise;
  auto future = responsePromise.get_future();

  Ptr<Request> request = New<Request>(
      requestId_++, /* lineNumberBegin = */ 0, vocabs_, std::move(input),
      std::move(segments), std::move(sourceRanges), std::move(responsePromise));

  batcher_.addWholeRequest(request);
  if (numWorkers_ == 0) {
    blocking_translate();
  } else {
    async_translate();
  }
  return future;
}

Service::~Service() {
#ifdef WITH_PTHREADS
  for (size_t workerId = 0; workerId < numWorkers_; workerId++) {

    Batch poison = Batch::poison();
    pcqueue_.ProduceSwap(poison);
  }

  for (size_t workerId = 0; workerId < numWorkers_; workerId++) {
    if (workers_[workerId].joinable()) {
      workers_[workerId].join();
    }
  }
#endif
}

} // namespace bergamot
} // namespace marian
