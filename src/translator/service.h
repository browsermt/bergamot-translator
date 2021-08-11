#ifndef SRC_BERGAMOT_SERVICE_H_
#define SRC_BERGAMOT_SERVICE_H_

#include <queue>
#include <thread>
#include <vector>

#include "data/types.h"
#include "response.h"
#include "response_builder.h"
#include "text_processor.h"
#include "threadsafe_batcher.h"
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
///
/// Pending decommission following https://github.com/mozilla/bergamot-translator/issues/15#issue-828300150.
class BlockingService {
 public:
  /// Construct a BlockingService with configuration loaded from an Options object. Does not require any keys, values to
  /// be set.
  BlockingService();

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

 private:
  ///  Numbering requests processed through this instance. Used to keep account of arrival times of the request. This
  ///  allows for using this quantity in priority based ordering.
  size_t requestId_;

  /// An aggregate batching pool associated with an async translating instance, which maintains an aggregate queue of
  /// requests compiled from  batching-pools of multiple translation models. Not thread-safe.
  AggregateBatchingPool batchingPool_;
};

/// Effectively a threadpool, providing an API to take a translation request of a source-text, paramaterized by
/// TranslationModel to be used for translation. Configurability on optional items for the Response corresponding to a
/// request is provisioned through ResponseOptions.
class AsyncService {
 public:
  /// Construct an AsyncService with configuration loaded from Options. Expects positive integer value for
  /// `cpu-threads`. Additionally requires options which configure AggregateBatchingPool.
  AsyncService(size_t numWorkers);

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

  const size_t numWorkers() { return numWorkers_; }

 private:
  /// Count of workers used for to run translation.
  size_t numWorkers_;

  std::vector<std::thread> workers_;

  /// Numbering requests processed through this instance. Used to keep account of arrival times of the request. This
  /// allows for using this quantity in priority based ordering.
  size_t requestId_;

  /// An aggregate batching pool associated with an async translating instance, which maintains an aggregate queue of
  /// requests compiled from  batching-pools of multiple translation models. The batching pool is wrapped around one
  /// object for thread-safety.
  GuardedBatchingPoolAccess<AggregateBatchingPool> safeBatchingPool_;
};

#ifndef WASM_COMPATIBLE_SOURCE
typedef AsyncService Service;
#else   // WASM_COMPATIBLE_SOURCE
typedef BlockingService Service;
#endif  // WASM_COMPATIBLE_SOURCE

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_SERVICE_H_
