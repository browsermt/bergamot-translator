#include "service.h"
#include "batch.h"
#include "definitions.h"

#include <string>
#include <utility>

namespace marian {
namespace bergamot {

Service::Service(Ptr<Options> options, MemoryBundle memoryBundle)
    : requestId_(0), options_(options),
      vocabs_(options, std::move(memoryBundle.vocabs)),
      text_processor_(vocabs_, options), batcher_(options),
      numWorkers_(options->get<int>("cpu-threads")),
      modelMemory_(std::move(memoryBundle.model)),
      shortlistMemory_(std::move(memoryBundle.shortlist))
#ifndef WASM_COMPATIBLE_SOURCE
      // 0 elements in PCQueue is illegal and can lead to failures. Adding a
      // guard to have at least one entry allocated. In the single-threaded
      // case, while initialized pcqueue_ remains unused.
      ,
      pcqueue_(std::max<size_t>(1, numWorkers_))
#endif
{

  if (numWorkers_ == 0) {
    build_translators(options, /*numTranslators=*/1);
    initialize_blocking_translator();
  } else {
    build_translators(options, numWorkers_);
    initialize_async_translators();
  }
}

void Service::build_translators(Ptr<Options> options, size_t numTranslators) {
  translators_.reserve(numTranslators);
  for (size_t cpuId = 0; cpuId < numTranslators; cpuId++) {
    marian::DeviceId deviceId(cpuId, DeviceType::cpu);
    translators_.emplace_back(deviceId, vocabs_, options, &modelMemory_,
                              &shortlistMemory_);
  }
}

void Service::initialize_blocking_translator() {
  translators_.back().initialize();
}

void Service::blocking_translate() {
  Batch batch;
  while (batcher_ >> batch) {
    auto &translator = translators_.back();
    translator.translate(batch);
  }
}

#ifndef WASM_COMPATIBLE_SOURCE
void Service::initialize_async_translators() {
  workers_.reserve(numWorkers_);

  for (size_t cpuId = 0; cpuId < numWorkers_; cpuId++) {
    auto &translator = translators_[cpuId];
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
#else  // WASM_COMPATIBLE_SOURCE
void Service::initialize_async_translators() {
  ABORT("Cannot run in async mode without multithreading.");
}

void Service::async_translate() {
  ABORT("Cannot run in async mode without multithreading.");
}
#endif // WASM_COMPATIBLE_SOURCE

void Service::translate(std::string &&input, 
                        std::function<void(Response &&)> &&callback) {
  ResponseOptions responseOptions; // Hardcode responseOptions for now
  return translate(std::move(input), std::move(callback), responseOptions);
}

std::vector<Response>
Service::translateMultiple(std::vector<std::string> &&inputs,
                           TranslationRequest translationRequest) {
  ResponseOptions responseOptions;

  // TODO(jerinphilip) Set options based on TranslationRequest, if and when it
  // becomes non-dummy.

  // We queue the individual Requests so they get compiled at batches to be
  // efficiently translated.
  std::vector<Response> responses;
  responses.resize(inputs.size());
  for (size_t i = 0;  i < inputs.size(); i++) {
    auto callback = [i, &responses](Response &&response){ responses[i] = std::move(response); };
    queueRequest(std::move(inputs[i]), std::move(callback), responseOptions);
  }

  // Dispatch is called once per request so compilation of sentences from
  // multiple Requests happen. Dispatch will trigger the callbacks and set values.
  dispatchTranslate();


  return responses;
}

void Service::queueRequest(std::string &&input,
                           std::function<void(Response &&)> &&callback, 
                           ResponseOptions responseOptions){
  Segments segments;
  AnnotatedText source(std::move(input));
  text_processor_.process(source, segments);

  ResponseBuilder responseBuilder(responseOptions, std::move(source), vocabs_,
                                  std::move(callback));
  Ptr<Request> request = New<Request>(requestId_++, std::move(segments),
                                      std::move(responseBuilder));

  batcher_.addWholeRequest(request);
}

void Service::translate(std::string &&input,
                        std::function<void(Response &&)> &&callback,
                        ResponseOptions responseOptions) {
  queueRequest(std::move(input), std::move(callback), responseOptions);
  dispatchTranslate();
}

void Service::dispatchTranslate() {
  if (numWorkers_ == 0) {
    blocking_translate();
  } else {
    async_translate();
  }
}

Service::~Service() {
#ifndef WASM_COMPATIBLE_SOURCE
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
