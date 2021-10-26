#ifndef SRC_BERGAMOT_SERVICE_H_
#define SRC_BERGAMOT_SERVICE_H_

#include <queue>
#include <thread>
#include <vector>

#include "cache.h"
#include "data/types.h"
#include "quality_estimator.h"
#include "response.h"
#include "response_builder.h"
#include "text_processor.h"
#include "threadsafe_batching_pool.h"
#include "translation_model.h"
#include "translator/parser.h"
#include "vocabs.h"

namespace marian {
namespace bergamot {

class BlockingService;
class AsyncService;

/// See AsyncService.
///
/// BlockingService is a not-threaded counterpart of AsyncService which can operate only in a blocking workflow (queue a
/// bunch of texts and optional args to translate, wait till the translation finishes).
class BlockingService {
 public:
  struct Config {
    bool cacheEnabled{false};  ///< Whether to enable cache or not.
    size_t cacheSize{2000};    ///< Size in History items to be stored in the cache. Loosely corresponds to sentences to
                               /// cache in the real world.
  };
  /// Construct a BlockingService with configuration loaded from an Options object. Does not require any keys, values to
  /// be set.
  BlockingService(const BlockingService::Config &config);

  /// Translate multiple text-blobs in a single *blocking* API call, providing ResponseOptions which applies across all
  /// text-blobs dictating how to construct Response. ResponseOptions can be used to enable/disable additional
  /// information like quality-scores, alignments etc.

  /// If you have async/multithread capabilities, it is recommended to work with AsyncService instead of this class.
  /// Note that due to batching differences and consequent floating-point rounding differences, this is not guaranteed
  /// to have the same output as AsyncService.

  /// @param [in] translationModel: TranslationModel to use for the request.
  /// @param [in] source: rvalue reference of the string to be translated
  /// @param [in] responseOptions: ResponseOptions indicating whether or not to include some member in the Response,
  /// also specify any additional configurable parameters.
  std::vector<Response> translateMultiple(std::shared_ptr<TranslationModel> translationModel,
                                          std::vector<std::string> &&source, const ResponseOptions &responseOptions);

  TranslationCache::Stats cacheStats() { return cache_.stats(); }

 private:
  ///  Numbering requests processed through this instance. Used to keep account of arrival times of the request. This
  ///  allows for using this quantity in priority based ordering.
  size_t requestId_;

  /// An aggregate batching pool associated with an async translating instance, which maintains an aggregate queue of
  /// requests compiled from  batching-pools of multiple translation models. Not thread-safe.
  AggregateBatchingPool batchingPool_;

  Config config_;

  TranslationCache cache_;
};

/// Effectively a threadpool, providing an API to take a translation request of a source-text, paramaterized by
/// TranslationModel to be used for translation. Configurability on optional items for the Response corresponding to a
/// request is provisioned through ResponseOptions.
class AsyncService {
 public:
  struct Config {
    size_t numWorkers;         ///< How many worker translation threads to spawn.
    bool cacheEnabled{false};  ///< Whether to enable cache or not.
    size_t cacheSize{2000};    ///< Size in History items to be stored in the cache. Loosely corresponds to sentences to
                               /// cache in the real world.
    size_t cacheMutexBuckets;  ///< Controls the granularity of locking to reduce contention by bucketing mutexes
                               ///< guarding cache entry read write. Optimal at min(core, numWorkers) assuming a
                               ///< reasonably large cache-size.
  };
  /// Construct an AsyncService with configuration loaded from Options. Expects positive integer value for
  /// `cpu-threads`. Additionally requires options which configure AggregateBatchingPool.
  AsyncService(const AsyncService::Config &config);

  /// Create a TranslationModel compatible with this instance of Service. Internally assigns how many replicas of
  /// backend needed based on worker threads set. See TranslationModel for documentation on other params.
  template <class ConfigType>
  Ptr<TranslationModel> createCompatibleModel(const ConfigType &config, MemoryBundle &&memory = MemoryBundle{}) {
    // @TODO: Remove this remove this dependency/coupling.
    return New<TranslationModel>(config, std::move(memory), /*replicas=*/config_.numWorkers);
  }

  /// With the supplied TranslationModel, translate an input. A Response is constructed with optional items set/unset
  /// indicated via ResponseOptions. Upon completion translation of the input, the client supplied callback is triggered
  /// with the constructed Response. Concurrent-calls to this function are safe.
  ///
  /// @param [in] translationModel: TranslationModel to use for the request.
  /// @param [in] source: rvalue reference of the string to be translated. This is available as-is to the client later
  /// in the Response corresponding to this call along with the translated-text and meta-data.
  /// @param [in] callback: A callback function provided by the client which accepts an rvalue of a Response.
  /// @param [in] responseOptions: Options indicating whether or not to include some member in the Response, also
  /// specify any additional configurable parameters.
  void translate(std::shared_ptr<TranslationModel> translationModel, std::string &&source, CallbackType callback,
                 const ResponseOptions &options = ResponseOptions());

  /// Thread joins and proper shutdown are required to be handled explicitly.
  ~AsyncService();

  TranslationCache::Stats cacheStats() { return cache_.stats(); }

 private:
  AsyncService::Config config_;

  std::vector<std::thread> workers_;

  /// Stores requestId of active request. Used to establish
  /// ordering among requests and logging/book-keeping.

  /// Numbering requests processed through this instance. Used to keep account of arrival times of the request. This
  /// allows for using this quantity in priority based ordering.
  size_t requestId_;

  /// An aggregate batching pool associated with an async translating instance, which maintains an aggregate queue of
  /// requests compiled from  batching-pools of multiple translation models. The batching pool is wrapped around one
  /// object for thread-safety.
  ThreadsafeBatchingPool<AggregateBatchingPool> safeBatchingPool_;

  TranslationCache cache_;
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_SERVICE_H_
