#include "service.h"

#include <string>
#include <utility>

#include "batch.h"
#include "definitions.h"

namespace marian {
namespace bergamot {

namespace {

// Make request process is shared between Async and Blocking workflow of translating.
Ptr<Request> makeRequest(size_t requestId, std::shared_ptr<TranslationModel> translationModel, std::string &&source,
                         CallbackType callback, const ResponseOptions &responseOptions) {
  Segments segments;
  AnnotatedText annotatedSource;

  translationModel->textProcessor().process(std::move(source), annotatedSource, segments);
  ResponseBuilder responseBuilder(responseOptions, std::move(annotatedSource), translationModel->vocabs(), callback);

  Ptr<Request> request = New<Request>(requestId, std::move(segments), std::move(responseBuilder));
  return request;
}

}  // namespace

BlockingService::BlockingService(const Ptr<Options> &options) : requestId_(0), batchingPool_(options) {}

std::vector<Response> BlockingService::translateMultiple(std::shared_ptr<TranslationModel> translationModel,
                                                         std::vector<std::string> &&sources,
                                                         const ResponseOptions &responseOptions) {
  std::vector<Response> responses;
  responses.resize(sources.size());

  for (size_t i = 0; i < sources.size(); i++) {
    auto callback = [i, &responses](Response &&response) { responses[i] = std::move(response); };  //
    Ptr<Request> request =
        makeRequest(requestId_++, translationModel, std::move(sources[i]), callback, responseOptions);
    batchingPool_.addRequest(translationModel, request);
  }

  Batch batch;
  // There's no need to do shutdown here because it's single threaded.
  Ptr<TranslationModel> model{nullptr};
  while (batchingPool_.generateBatch(model, batch)) {
    translateBatch(/*deviceId=*/0, model, batch);
  }

  return responses;
}

AsyncService::AsyncService(const Ptr<Options> &options)
    : requestId_(0), numWorkers_(std::max<int>(1, options->get<int>("cpu-threads"))), safeBatchingPool_(options) {
  workers_.reserve(numWorkers_);
  for (size_t cpuId = 0; cpuId < numWorkers_; cpuId++) {
    workers_.emplace_back([cpuId, this] {
      Batch batch;
      // Run thread mainloop
      Ptr<TranslationModel> translationModel{nullptr};
      while (safeBatchingPool_.generateBatch(translationModel, batch)) {
        translateBatch(cpuId, translationModel, batch);
      }
    });
  }
}

AsyncService::~AsyncService() {
  safeBatchingPool_.shutdown();
  for (std::thread &worker : workers_) {
    assert(worker.joinable());
    worker.join();
  }
}

void AsyncService::translate(std::shared_ptr<TranslationModel> translationModel, std::string &&source,
                             CallbackType callback, const ResponseOptions &responseOptions) {
  Ptr<Request> request = makeRequest(requestId_++, translationModel, std::move(source), callback, responseOptions);
  safeBatchingPool_.addRequest(translationModel, request);
}

}  // namespace bergamot
}  // namespace marian
