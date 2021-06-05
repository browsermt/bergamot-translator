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
      text_processor_(options, vocabs_, std::move(memoryBundle.ssplitPrefixFile)),
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
  for (auto &input : inputs) {
    std::future<Response> inputResponse = queueRequest(std::move(input), responseOptions);
    responseFutures.push_back(std::move(inputResponse));
  }

  // Dispatch is called once per request so compilation of sentences from
  // multiple Requests happen.
  blockIfWASM();

  // Now wait for all Requests to complete, the future to fire and return the
  // compiled Responses, we can probably return the future, but WASM quirks(?).
  std::vector<Response> responses;
  for (auto &future : responseFutures) {
    future.wait();
    responses.push_back(std::move(future.get()));
  }

  return responses;
}

std::future<Response> Service::queueRequest(std::string &&input, ResponseOptions responseOptions) {
  Segments segments;
  AnnotatedText source;

  text_processor_.process(std::move(input), source, segments);

  std::promise<Response> responsePromise;
  auto future = responsePromise.get_future();

  ResponseBuilder responseBuilder(responseOptions, std::move(source), vocabs_, std::move(responsePromise));
  Ptr<Request> request = New<Request>(requestId_++, std::move(segments), std::move(responseBuilder));

  batcher_.addWholeRequest(request);
  return future;
}

std::future<Response> Service::translate(std::string &&input, ResponseOptions responseOptions) {
  std::future<Response> future = queueRequest(std::move(input), responseOptions);
  blockIfWASM();
  return future;
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
