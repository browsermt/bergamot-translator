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
      batcher_(options),
      numWorkers_(std::max<int>(1, options->get<int>("cpu-threads"))) {
  translationModel_ = New<TranslationModel>(options_, std::move(memoryBundle), /*replicas=*/numWorkers_);
#ifndef WASM_COMPATIBLE_SOURCE
  workers_.reserve(numWorkers_);
  for (size_t cpuId = 0; cpuId < numWorkers_; cpuId++) {
    workers_.emplace_back([cpuId, this] {
      Batch batch;
      // Run thread mainloop
      Ptr<TranslationModel> translationModel{nullptr};
      while (batcher_.generateBatch(translationModel, batch)) {
        translateBatch(cpuId, translationModel, batch);
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
    queueRequest(translationModel_, std::move(inputs[i]), std::move(callback), responseOptions);
  }

  Batch batch;
  // There's no need to do shutdown here because it's single threaded.
  while (batcher_.generateBatch(batch)) {
    translateBatch(/*deviceId=*/0, translationModel_, batch);
  }

  return responses;
}
#endif

void Service::queueRequest(Ptr<TranslationModel> translationModel, std::string &&input, CallbackType &&callback,
                           ResponseOptions responseOptions) {
  Segments segments;
  AnnotatedText source;

  translationModel->textProcessor().process(std::move(input), source, segments);

  ResponseBuilder responseBuilder(responseOptions, std::move(source), translationModel->vocabs(), std::move(callback));
  Ptr<Request> request = New<Request>(requestId_++, std::move(segments), std::move(responseBuilder));

  batcher_.addRequest(translationModel, request);
}

void Service::translate(std::string &&input, CallbackType &&callback, ResponseOptions responseOptions) {
  queueRequest(translationModel_, std::move(input), std::move(callback), responseOptions);
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
