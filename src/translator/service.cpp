#include "service.h"

#include <string>
#include <utility>

#include "batch.h"
#include "definitions.h"

namespace marian {
namespace bergamot {

BlockingService::BlockingService(const Ptr<Options> &options) : requestId_(0), batchingPool_(options) {}

std::vector<Response> BlockingService::translateMultiple(std::shared_ptr<TranslationModel> translationModel,
                                                         std::vector<std::string> &&sources,
                                                         const ResponseOptions &responseOptions) {
  std::vector<Response> responses;
  responses.resize(sources.size());

  for (size_t i = 0; i < sources.size(); i++) {
    auto callback = [i, &responses](Response &&response) { responses[i] = std::move(response); };  //
    Ptr<Request> request =
        translationModel->makeRequest(requestId_++, std::move(sources[i]), callback, responseOptions);
    batchingPool_.enqueueRequest(translationModel, request);
  }

  Batch batch;
  Ptr<TranslationModel> model{nullptr};
  while (batchingPool_.generateBatch(model, batch)) {
    model->translateBatch(/*deviceId=*/0, batch);
  }

  return responses;
}

void BlockingService::translate(std::shared_ptr<TranslationModel> translationModel, std::string &&source,
                                CallbackType callback, const ResponseOptions &responseOptions) {
  // Tests can go haywire in blocking path. The ABORT below prevents future attempts with a useful error message at
  // supporting this function in BRT.
  ABORT("translate(...) is not supported in blocking platform. Please avoid attempt to call this function.");

  // The following code might work, but there are no guarantees. This is currently kept to encourage writing better
  // modular code which can inter-operate between threaded (blocking) and non-threaded (asynchronous) workflow. If
  // compilation fails while the below units are active, you are enforcing threaded-only.

  // Push request onto the batching data-structure
  Ptr<Request> request = translationModel->makeRequest(requestId_++, std::move(source), callback, responseOptions);
  batchingPool_.enqueueRequest(translationModel, request);

  // Pull batches compiled from existing requests from the batching-data structure.
  Batch batch;
  Ptr<TranslationModel> model{nullptr};
  while (batchingPool_.generateBatch(model, batch)) {
    model->translateBatch(/*deviceId=*/0, batch);
  }
  // ^ When the last sentence is done translating, callback planted in makeRequest will fire the necessary
  // post-processing. If there is a single sentence, there can be a legal batch, the above loop will complete.
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
        translationModel->translateBatch(cpuId, batch);
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
  Ptr<Request> request = translationModel->makeRequest(requestId_++, std::move(source), callback, responseOptions);
  safeBatchingPool_.enqueueRequest(translationModel, request);
}

std::vector<Response> AsyncService::translateMultiple(std::shared_ptr<TranslationModel> translationModel,
                                                      std::vector<std::string> &&source,
                                                      const ResponseOptions &responseOptions) {
  ABORT("Blocking multiple text workflow is not supported on AsyncService. Please check the call-site.");
};

}  // namespace bergamot
}  // namespace marian
