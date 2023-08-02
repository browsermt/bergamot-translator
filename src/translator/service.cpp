#include "service.h"

#include <regex>
#include <string>
#include <utility>

#include "batch.h"
#include "byte_array_util.h"
#include "definitions.h"

namespace marian {
namespace bergamot {

namespace {

// Replacement_fn taken from https://stackoverflow.com/questions/3418231/replace-part-of-a-string-with-another-string
// Sue me.
size_t CountOccurrences(std::string_view s, std::string_view needle) {
  size_t res = 0;
  size_t pos = 0;
  while ((pos = s.find(needle, pos)) != std::string_view::npos) {
    ++res;
    pos += needle.size();
  }
  return res;
}

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

std::optional<TranslationCache> makeOptionalCache(size_t size, size_t mutexBuckets) {
  return size > 0 ? std::make_optional<TranslationCache>(size, mutexBuckets) : std::nullopt;
}

// https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
// No string std::format/ std::vformat until c++20 :( so we use snprintf based solution
std::string string_format(const std::string &format, const std::string &src, const std::string &trg) {
  // Extra space for '\0'
  size_t size = static_cast<size_t>(std::snprintf(nullptr, 0, format.c_str(), src.c_str(), trg.c_str()) + 1);
  auto buf = std::make_unique<char[]>(size);
  std::snprintf(buf.get(), size, format.c_str(), src.c_str(), trg.c_str());
  return std::string(buf.get(), buf.get() + size - 1);  // We don't want the '\0' inside
}

}  // namespace

BlockingService::BlockingService(const BlockingService::Config &config)
    : config_(config),
      requestId_(0),
      batchingPool_(),
      cache_(makeOptionalCache(config.cacheSize, /*mutexBuckets = */ 1)),
      logger_(config.logger) {}

std::vector<Response> BlockingService::translateMultiple(std::shared_ptr<TranslationModel> translationModel,
                                                         std::vector<std::string> &&sources,
                                                         const std::vector<ResponseOptions> &responseOptions) {
  std::vector<HTML> htmls;
  for (size_t i = 0; i < sources.size(); i++) {
    htmls.emplace_back(std::move(sources[i]), responseOptions[i].HTML);
  }
  std::vector<Response> responses = translateMultipleRaw(translationModel, std::move(sources), responseOptions);
  for (size_t i = 0; i < responses.size(); i++) {
    htmls[i].restore(responses[i]);
  }

  return responses;
}

std::vector<Response> BlockingService::translateMultipleRaw(std::shared_ptr<TranslationModel> translationModel,
                                                            std::vector<std::string> &&sources,
                                                            const std::vector<ResponseOptions> &responseOptions) {
  std::vector<Response> responses;
  responses.resize(sources.size());

  for (size_t i = 0; i < sources.size(); i++) {
    auto callback = [i, &responses](Response &&response) { responses[i] = std::move(response); };  //
    Ptr<Request> request =
        translationModel->makeRequest(requestId_++, std::move(sources[i]), callback, responseOptions[i], cache_);
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
                                                     const std::vector<ResponseOptions> &responseOptions) {
  std::vector<HTML> htmls;
  for (size_t i = 0; i < sources.size(); i++) {
    htmls.emplace_back(std::move(sources[i]), responseOptions[i].HTML);
  }

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

    Ptr<Request> request =
        second->makePivotRequest(requestId_++, std::move(intermediate), callback, responseOptions[i], cache_);
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

  for (size_t i = 0; i < finalResponses.size(); i++) {
    htmls[i].restore(finalResponses[i]);
  }

  return finalResponses;
}

AsyncService::AsyncService(const AsyncService::Config &config)
    : requestId_(0),
      config_(config),
      safeBatchingPool_(),
      cache_(makeOptionalCache(config_.cacheSize, /*mutexBuckets=*/config_.numWorkers)),
      logger_(config.logger) {
  if (config_.gpuWorkers.empty()) {
    ABORT_IF(config_.numWorkers == 0, "Number of workers should be at least 1 in a threaded workflow");
    workers_.reserve(config_.numWorkers);
  } else {
    if (config_.numWorkers != 0) LOG(info, "Unable to mix GPU and CPU workers, using GPU workers only...");
    workers_.reserve(config_.gpuWorkers.size());
    // VERY VERY HACKY. EVERYTHING USES NUM_WORKERS AS A REFERENCE FOR THE NUMBER OF WORKERS,
    // REFACTOR TO USE gpuWorkers directly...
    config_.numWorkers = config_.gpuWorkers.size();
  }
  // Initiate terminology map if present
  if (!config_.terminologyFile.empty()) {
    // Create an input filestream
    std::ifstream myFile(config_.terminologyFile);

    // Make sure the file is open
    if (!myFile.is_open()) throw std::runtime_error("Could not open file: " + config_.terminologyFile);
    std::string line;
    std::unordered_map<std::string, std::string> tempmap;  // Read in the TSV to here
    while (std::getline(myFile, line)) {
      // Create a stringstream of the current line
      std::stringstream ss(line);

      std::string srcword;
      std::string replacementword;
      getline(ss, srcword, '\t');
      getline(ss, replacementword, '\n');  // BEWARE of windows file ndings
      tempmap.insert({srcword, replacementword});
    }

    // Close file
    myFile.close();

    // Load the terminology
    setTerminology(tempmap, config.terminologyForce);
  }

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

void AsyncService::setTerminology(std::unordered_map<std::string, std::string> &terminology, bool forceTerminology) {
  terminologyMap_.clear();  // Clear old terminology map
  // Copy the map. Since we might be coming from python space anyways. Also take care of force here
  for (auto const &[key, val] : terminology) {
    if (!forceTerminology) {
      terminologyMap_[key] = string_format(config_.format, key, val);
    } else {
      // @TODO it seems like removing the tags forces the model to copy which is
      // I guess just as good and more reliable. In that case we just don't tell the model
      // what the original source is and it just has no choice BUT to generate the target.
      terminologyMap_[key] = val;
    }
  }

  // Testing
  if (config_.logger.level == "debug") {
    std::cerr << "Printing out terminology...:" << std::endl;
    for (auto &&item : terminologyMap_) {
      std::cerr << item.first << " " << item.second << std::endl;
    }
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
  Ptr<HTML> html = std::make_shared<HTML>(std::move(source), responseOptions.HTML);
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
      html->restore(finalResponse);
      clientCallback(std::move(finalResponse));
    };

    // Second call.
    Ptr<Request> request =
        second->makePivotRequest(requestId_++, std::move(intermediate), joiningCallback, responseOptions, cache_);
    safeBatchingPool_.enqueueRequest(second, request);
  };

  // First call.
  translateRaw(first, std::move(source), internalCallback, responseOptions);
}

void AsyncService::translate(std::shared_ptr<TranslationModel> translationModel, std::string &&source,
                             CallbackType callback, const ResponseOptions &responseOptions) {
  // Producer thread, a call to this function adds new work items. If batches are available, notifies workers waiting.
  // Tagging
  if (!terminologyMap_.empty()) source = ReplaceTerminology(std::move(source), terminologyMap_);

  Ptr<HTML> html = std::make_shared<HTML>(std::move(source), responseOptions.HTML);
  auto internalCallback = [html, callback](Response &&response) {
    html->restore(response);
    callback(std::move(response));
  };
  translateRaw(translationModel, std::move(source), internalCallback, responseOptions);
}

void AsyncService::translateRaw(std::shared_ptr<TranslationModel> translationModel, std::string &&source,
                                CallbackType callback, const ResponseOptions &responseOptions) {
  // Producer thread, a call to this function adds new work items. If batches are available, notifies workers waiting.
  Ptr<Request> request =
      translationModel->makeRequest(requestId_++, std::move(source), callback, responseOptions, cache_);
  safeBatchingPool_.enqueueRequest(translationModel, request);
}

}  // namespace bergamot
}  // namespace marian
