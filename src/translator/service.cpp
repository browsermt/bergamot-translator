#include "service.h"

#include <string>
#include <utility>

#include "batch.h"
#include "definitions.h"

namespace marian {
namespace bergamot {

Service::Service(Ptr<Options> options, MemoryBundle memoryBundle)
    : requestId_(0),
      options_(options),
      vocabs_(options, std::move(memoryBundle.vocabs)),
      text_processor_(vocabs_, options),
      batcher_(options),
      numWorkers_(std::max<int>(1, options->get<int>("cpu-threads"))),
      modelMemory_(std::move(memoryBundle.model)),
      shortlistMemory_(std::move(memoryBundle.shortlist))
#ifdef WASM_COMPATIBLE_SOURCE
      ,
      blocking_translator_(DeviceId(0, DeviceType::cpu), vocabs_, options_, &modelMemory_, &shortlistMemory_)
#endif
{
#ifdef WASM_COMPATIBLE_SOURCE
  blocking_translator_.initialize();
#else
  workers_.reserve(numWorkers_);
  for (size_t cpuId = 0; cpuId < numWorkers_; cpuId++) {
    workers_.emplace_back([cpuId, this] {
      marian::DeviceId deviceId(cpuId, DeviceType::cpu);
      BatchTranslator translator(deviceId, vocabs_, options_, &modelMemory_, &shortlistMemory_);
      translator.initialize();
      Batch batch;
      // Run thread mainloop
      while (batcher_ >> batch) {
        translator.translate(batch);
      }
    });
  }
#endif
}

void Service::blockIfWASM() {
#ifdef WASM_COMPATIBLE_SOURCE
  Batch batch;
  // There's no need to do shutdown here because it's single threaded.
  while (batcher_ >> batch) {
    blocking_translator_.translate(batch);
  }
#endif
}

std::vector<Response> Service::translateMultiple(std::vector<std::string> &&inputs, ResponseOptions responseOptions) {
  // We queue the individual Requests so they get compiled at batches to be
  // efficiently translated.
  std::vector<std::future<Response>> responseFutures;
  for (size_t i = 0; i < inputs.size(); i++) {
    // https://stackoverflow.com/a/33437008/4565794
    // @jerinphilip is not proud of this.
    Ptr<std::promise<Response>> responsePromise = New<std::promise<Response>>();
    responseFutures.push_back(std::move(responsePromise->get_future()));
    auto callback = [responsePromise](Response &&response) { responsePromise->set_value(std::move(response)); };
    queueRequest(std::move(inputs[i]), std::move(callback), responseOptions);
  }

  // Dispatch is called once per request so compilation of sentences from
  // multiple Requests happen.
  blockIfWASM();

  std::vector<Response> responses;
  responses.reserve(responseFutures.size());
  for (auto &responseFuture : responseFutures) {
    responseFuture.wait();
    responses.push_back(std::move(responseFuture.get()));
  }

  return responses;
}

void Service::queueRequest(std::string &&input, std::function<void(Response &&)> &&callback,
                           ResponseOptions responseOptions) {
  Segments segments;
  AnnotatedText source(std::move(input));
  text_processor_.process(source, segments);

  ResponseBuilder responseBuilder(responseOptions, std::move(source), vocabs_, std::move(callback));
  Ptr<Request> request = New<Request>(requestId_++, std::move(segments), std::move(responseBuilder));

  batcher_.addWholeRequest(request);
}

void Service::translate(std::string &&input, std::function<void(Response &&)> &&callback,
                        ResponseOptions responseOptions) {
  queueRequest(std::move(input), std::move(callback), responseOptions);
  blockIfWASM();
}

Service::~Service() {
  batcher_.shutdown();
#ifndef WASM_COMPATIBLE_SOURCE
  for (std::thread &worker : workers_) {
    assert(worker.joinable());
    worker.join();
  }
#endif
}

}  // namespace bergamot
}  // namespace marian
