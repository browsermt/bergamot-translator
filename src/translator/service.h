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

///  This is intended to be similar to the ones  provided for training or decoding in ML pipelines with the following
///  additional capabilities:
///
///  1. Provision of a request -> response based translation flow unlike the usual a line based translation or decoding
///  provided in most ML frameworks.
///  2. Internal handling of normalization etc which changes source text to provide to client translation
///  meta-information like alignments consistent with the unnormalized input text.
///  3. The API splits each text entry into sentences internally, which are then translated independent of each other.
///  The translated sentences are then joined back together and returned in Response.
///
/// Service exposes methods to instantiate from a string configuration (which can cover most translators) and to
/// translate an incoming blob of text.
///
/// Optionally Service can be initialized by also passing bytearray memories for purposes of efficiency (which defaults
/// to empty and then reads from file supplied through config).

class BlockingService;
class AsyncService;

class BlockingService {
 public:
  BlockingService(const std::shared_ptr<Options> &options);
  BlockingService(const std::string &config) : BlockingService(parseOptions(config, /*validate=*/false)){};

  /// Translate multiple text-blobs in a single *blocking* API call, providing ResponseOptions which applies across all
  /// text-blobs dictating how to construct Response. ResponseOptions can be used to enable/disable additional
  /// information like quality-scores, alignments etc.

  /// All texts are combined to efficiently construct batches together providing speedups compared to calling
  /// translate() independently on individual text-blob. Note that there will be minor differences in output when
  /// text-blobs are individually translated due to approximations but similar quality nonetheless. If you have
  /// async/multithread capabilities, it is recommended to work with AsyncService.

  /// @param [in] translationModel: TranslationModel to use for the request.
  /// @param [in] source: rvalue reference of the string to be translated
  /// @param [in] responseOptions: ResponseOptions indicating whether or not to include some member in the Response,
  /// also specify any additional configurable parameters.
  std::vector<Response> translateMultiple(std::shared_ptr<TranslationModel> translationModel,
                                          std::vector<std::string> &&source, const ResponseOptions &responseOptions);

  /// This is not supported on BlockingService. The simplest way to not make a mess of ifdef WASM_COMPATIBLE_SOURCE
  /// propogating to source is to have an equivalent for this function.
  void translate(std::shared_ptr<TranslationModel> translationModel, std::string &&source, CallbackType callback,
                 const ResponseOptions &options = ResponseOptions());

 private:
  ///  Numbering requests processed through this instance. Used to keep account of arrival times of the request. This
  ///  allows for using this quantity in priority based ordering.
  size_t requestId_;

  /// An aggregate batching pool associated with an async translating instance, which maintains an aggregate queue of
  /// requests compiled from  batching-pools of multiple translation models. Not thread-safe.
  AggregateBatchingPool batchingPool_;
};

class AsyncService {
 public:
  AsyncService(const std::shared_ptr<Options> &options);
  AsyncService(const std::string &config) : AsyncService(parseOptions(config, /*validate=*/false)){};

  /// Translate an input, providing Options to construct Response. This is useful when one has to set/unset alignments
  /// or quality in the Response to save compute spent in constructing these objects. Concurrent-calls to this function
  /// are safe and possible.
  ///
  /// @param [in] translationModel: TranslationModel to use for the request.
  /// @param [in] source: rvalue reference of the string to be translated
  /// @param [in] callback: A callback function provided by the client which accepts an rvalue of a Response. Called on
  /// successful construction of a Response following completion of translation of source by worker threads.
  /// @param [in] responseOptions: Options indicating whether or not to include some member in the Response, also
  /// specify any additional configurable parameters.
  void translate(std::shared_ptr<TranslationModel> translationModel, std::string &&source, CallbackType callback,
                 const ResponseOptions &options = ResponseOptions());

  /// UNSUPPORTED. For parity.
  std::vector<Response> translateMultiple(std::shared_ptr<TranslationModel> translationModel,
                                          std::vector<std::string> &&source, const ResponseOptions &responseOptions);
  /// Thread joins and proper shutdown are required to be handled explicitly.
  ~AsyncService();

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
