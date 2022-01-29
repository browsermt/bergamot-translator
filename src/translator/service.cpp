#include "service.h"

#include <string>
#include <utility>

#include "batch.h"
#include "byte_array_util.h"
#include "definitions.h"

namespace marian {
namespace bergamot {

namespace {

// Combines two responses with first.target == second.source mapping alignments etc accordingly.
// There are several constraints which are matched by only the pivoting workflow in <>Service source, therefore this
// function is not for external use and in a hidden namespace.
Response combine(Response &&first, Response &&second) {
  Response combined;

  // Compute alignment first using internal matrices and mappings.
  if (first.alignments.size()) {
    combined.alignments = remapAlignments(first, second);
  }

  combined.source = std::move(first.source);
  combined.target = std::move(second.target);
  combined.qualityScores = std::move(second.qualityScores);

  return combined;
}

}  // namespace

BlockingService::BlockingService(const BlockingService::Config &config)
    : config_(config),
      requestId_(0),
      batchingPool_(),
      cache_(config.cacheSize, /*mutexBuckets=*/1),
      logger_(config.logger) {}

std::vector<Response> BlockingService::translateMultiple(std::shared_ptr<TranslationModel> translationModel,
                                                         std::vector<std::string> &&sources,
                                                         const ResponseOptions &responseOptions) {
  HTML html;

  std::vector<HTML> htmls(sources.size());
  std::vector<Response> responses(sources.size());

  size_t j = 0;
  for (size_t i = 0; i < sources.size(); ++i) {
    if (!responseOptions.HTML) {
      sources[j++] = std::move(sources[i]);
    } else if (auto err = htmls[i].parse(sources[i])) {
      responses[i].error = err->message;
    } else {
      sources[j++] = std::move(sources[i]);
    }
  }
  sources.resize(j);  // Clip off all the failed parses

  std::vector<Response> translationResponses =
      translateMultipleRaw(translationModel, std::move(sources), responseOptions);

  auto it = translationResponses.begin();
  for (size_t i = 0; i < responses.size(); ++i) {
    if (responses[i].ok()) {
      responses[i] = std::move(*it++);
      if (auto err = htmls[i].restore(responses[i])) {
        responses[i].error = err->message;
      }
    }
  }
  assert(it == translationResponses.end());

  return responses;
}

std::vector<Response> BlockingService::translateMultipleRaw(std::shared_ptr<TranslationModel> translationModel,
                                                            std::vector<std::string> &&sources,
                                                            const ResponseOptions &responseOptions) {
  std::vector<Response> responses(sources.size());

  for (size_t i = 0; i < sources.size(); i++) {
    auto callback = [i, &responses](Response &&response) { responses[i] = std::move(response); };  //
    TranslationCache *cache = config_.cacheEnabled ? &cache_ : nullptr;
    Ptr<Request> request =
        translationModel->makeRequest(requestId_++, std::move(sources[i]), callback, responseOptions, cache);
    batchingPool_.enqueueRequest(translationModel, request);
  }

  Batch batch;
  Ptr<TranslationModel> model{nullptr};
  while (batchingPool_.generateBatch(model, batch)) {
    model->translateBatch(/*deviceId=*/0, batch);
  }

  return responses;
}

std::vector<Response> BlockingService::pivotMultiple(std::shared_ptr<TranslationModel> first,
                                                     std::shared_ptr<TranslationModel> second,
                                                     std::vector<std::string> &&sources,
                                                     const ResponseOptions &responseOptions) {
  // Translate source to pivots. This is same as calling translateMultiple.
  std::vector<Response> sourcesToPivots;
  sourcesToPivots = translateMultipleRaw(first, std::move(sources), responseOptions);

  // Translate pivots to targets, after we have outputs at pivot from first round. We cannot use translateMultiple here
  // because need consistency at pivot on both sides.
  std::vector<Response> pivotsToTargets;
  pivotsToTargets.resize(sourcesToPivots.size());

  for (size_t i = 0; i < sourcesToPivots.size(); i++) {
    AnnotatedText intermediate =
        sourcesToPivots[i].target;  // We cannot eliminate this copy, as we need two versions of intermediate. Holding
                                    // it in allows further use in makePivotRequest
    auto callback = [i, &pivotsToTargets](Response &&response) { pivotsToTargets[i] = std::move(response); };  //

    TranslationCache *cache = config_.cacheEnabled ? &cache_ : nullptr;
    Ptr<Request> request =
        second->makePivotRequest(requestId_++, std::move(intermediate), callback, responseOptions, cache);
    batchingPool_.enqueueRequest(second, request);
  }

  Batch batch;
  Ptr<TranslationModel> model{nullptr};
  while (batchingPool_.generateBatch(model, batch)) {
    model->translateBatch(/*deviceId=*/0, batch);
  }

  // Combine both sides. They're associated by indices.
  std::vector<Response> finalResponses;
  for (size_t i = 0; i < sourcesToPivots.size(); i++) {
    Response finalResponse = combine(std::move(sourcesToPivots[i]), std::move(pivotsToTargets[i]));
    finalResponses.push_back(std::move(finalResponse));
  }

  return finalResponses;
}

AsyncService::AsyncService(const AsyncService::Config &config)
    : requestId_(0),
      config_(config),
      safeBatchingPool_(),
      cache_(config_.cacheSize, config_.cacheMutexBuckets),
      logger_(config.logger) {
  ABORT_IF(config_.numWorkers == 0, "Number of workers should be at least 1 in a threaded workflow");
  workers_.reserve(config_.numWorkers);
  for (size_t cpuId = 0; cpuId < config_.numWorkers; cpuId++) {
    workers_.emplace_back([cpuId, this] {
      // Consumer thread main-loop. Note that this is an infinite-loop unless the monitor is explicitly told to
      // shutdown, which happens in the destructor for this class.
      Batch batch;
      Ptr<TranslationModel> translationModel{nullptr};
      while (safeBatchingPool_.generateBatch(translationModel, batch)) {
        translationModel->translateBatch(cpuId, batch);
      }
    });
  }
}

void AsyncService::clear() { safeBatchingPool_.clear(); }

AsyncService::~AsyncService() {
  safeBatchingPool_.shutdown();
  for (std::thread &worker : workers_) {
    assert(worker.joinable());
    worker.join();
  }
  workers_.clear();
}

void AsyncService::pivot(std::shared_ptr<TranslationModel> first, std::shared_ptr<TranslationModel> second,
                         std::string &&source, CallbackType clientCallback, const ResponseOptions &responseOptions) {
  Ptr<HTML> html = std::make_shared<HTML>();
  // This is callback chaining or CPS due to async.

  // We create a callback which feeds the result of first into a second translation (internalCallback), which is
  // supplied with a callback (joiningCallback) which merges both results and creates our final response.
  //

  auto internalCallback = [this, clientCallback, second, responseOptions, html](Response &&sourceToPivot) {
    // We cannot eliminate the following copy, as we need two versions of intermediate. Holding
    // it in a copy allows moving the response into the lambda below.

    AnnotatedText intermediate = sourceToPivot.target;

    // https://stackoverflow.com/a/65606554/4565794
    // Move semantics only work on mutable lambdas, and can only be done once. It's only once in our case, so issok.
    auto joiningCallback = [this, sourceToPivot = std::move(sourceToPivot), clientCallback,
                            html](Response &&pivotToTarget) mutable {
      // We have both Responses at this callback, sourceToPivot is moved in, second half will be available when
      // complete.
      Response finalResponse = combine(std::move(sourceToPivot), std::move(pivotToTarget));

      // Sentences should be consistent now, give way to client.
      if (auto err = html->restore(finalResponse)) {
        clientCallback(Response::withError(std::move(err->message)));
      } else {
        clientCallback(std::move(finalResponse));
      }
    };

    // Second call.
    TranslationCache *cache = config_.cacheEnabled ? &cache_ : nullptr;
    Ptr<Request> request =
        second->makePivotRequest(requestId_++, std::move(intermediate), joiningCallback, responseOptions, cache);
    safeBatchingPool_.enqueueRequest(second, request);
  };

  if (responseOptions.HTML) {
    if (auto err = html->parse(source)) {
      clientCallback(Response::withError(std::move(err->message)));
      return;
    }
  }

  // First call.
  translateRaw(first, std::move(source), internalCallback, responseOptions);
}

void AsyncService::translate(std::shared_ptr<TranslationModel> translationModel, std::string &&source,
                             CallbackType callback, const ResponseOptions &responseOptions) {
  // Producer thread, a call to this function adds new work items. If batches are available, notifies workers waiting.
  Ptr<HTML> html = std::make_shared<HTML>();

  if (responseOptions.HTML) {
    if (auto err = html->parse(source)) {
      callback(Response::withError(std::move(err->message)));
      return;
    }
  }

  auto internalCallback = [html, callback](Response &&response) {
    if (auto err = html->restore(response)) {
      callback(Response::withError(std::move(err->message)));
    } else {
      callback(std::move(response));
    }
  };

  translateRaw(translationModel, std::move(source), internalCallback, responseOptions);
}

void AsyncService::translateRaw(std::shared_ptr<TranslationModel> translationModel, std::string &&source,
                                CallbackType callback, const ResponseOptions &responseOptions) {
  // Producer thread, a call to this function adds new work items. If batches are available, notifies workers waiting.
  TranslationCache *cache = config_.cacheEnabled ? &cache_ : nullptr;
  Ptr<Request> request =
      translationModel->makeRequest(requestId_++, std::move(source), callback, responseOptions, cache);
  safeBatchingPool_.enqueueRequest(translationModel, request);
}

}  // namespace bergamot
}  // namespace marian
