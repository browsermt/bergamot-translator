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
      shortlistMemory_(std::move(memoryBundle.shortlist)),
      cache_(options->get<size_t>("cache-size") * 1024 * 1024  // sizeInBytes, x 1024 x 1024 = sizeInMB
             )
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

#ifdef WASM_COMPATIBLE_SOURCE
std::vector<Response> Service::translateMultiple(std::vector<std::string> &&inputs, ResponseOptions responseOptions) {
  // We queue the individual Requests so they get compiled at batches to be
  // efficiently translated.
  std::vector<Response> responses;
  responses.resize(inputs.size());

  for (size_t i = 0; i < inputs.size(); i++) {
    auto callback = [i, &responses](Response &&response) { responses[i] = std::move(response); };  //
    queueRequest(std::move(inputs[i]), std::move(callback), responseOptions);
  }

  Batch batch;
  // There's no need to do shutdown here because it's single threaded.
  while (batcher_ >> batch) {
    blocking_translator_.translate(batch);
  }

  return responses;
}
#endif

void Service::queueRequest(std::string &&input, std::function<void(Response &&)> &&callback,
                           ResponseOptions responseOptions) {
  Segments segments;
  AnnotatedText source;

  text_processor_.process(std::move(input), source, segments);

  ResponseBuilder responseBuilder(responseOptions, std::move(source), vocabs_, std::move(callback));
  Ptr<Request> request = New<Request>(requestId_++, std::move(segments), std::move(responseBuilder), cache_);

  batcher_.addWholeRequest(request);
}

void Service::translate(std::string &&input, std::function<void(Response &&)> &&callback,
                        ResponseOptions responseOptions) {
  queueRequest(std::move(input), std::move(callback), responseOptions);
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
