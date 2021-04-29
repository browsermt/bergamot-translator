#ifndef SRC_BERGAMOT_SERVICE_H_
#define SRC_BERGAMOT_SERVICE_H_

#include "TranslationRequest.h"
#include "batch_translator.h"
#include "batcher.h"
#include "data/types.h"
#include "response.h"
#include "response_builder.h"
#include "text_processor.h"
#include "translator/parser.h"

#ifndef WASM_COMPATIBLE_SOURCE
#include "pcqueue.h"
#endif

#include <queue>
#include <vector>

namespace marian {
namespace bergamot {

/// Service offers methods create an asynchronous translation service. This is
/// intended to be similar to the ones provided for training or decoding in ML
/// pipelines with the following additional capabilities:
///
///  1. Provision of a request -> response based translation flow unlike the
///  usual a line based translation or decoding provided in most ML frameworks.
///  2. Internal handling of normalization etc which changes source text to
///  provide to client translation meta-information like alignments consistent
///  with the unnormalized input text.
///
/// Service exposes methods to instantiate the service from a string
/// configuration (which can cover most translators) and to translate an
/// incoming blob of text.
///
///
/// An example use of this API looks as follows:
/// ```cpp
///  options = ...;
///  service = Service(options);
///  std::string input_text = "Hello World";
///  std::future<Response>
///      responseFuture = service.translate(std::move(input_text));
///  responseFuture.wait(); // Wait until translation has completed.
///  Response response(std::move(response.get());
///
/// // Do things with response.
/// ```
///
/// Optionally Service can be initialized by also passing model_memory for
/// purposes of efficiency (which defaults to nullpointer and then reads from
/// file supplied through config).
class Service {

public:
  /// @param options Marian options object
  /// @param modelMemory byte array (aligned to 256!!!) that contains the bytes
  /// of a model.bin. Optional, defaults to nullptr when not used
  /// @param shortlistMemory byte array of shortlist (aligned to 64)
  explicit Service(Ptr<Options> options, AlignedMemory modelMemory,
                   AlignedMemory shortlistMemory);

  /// Construct Service purely from Options. This expects options which
  /// marian-decoder expects to be set for loading model shortlist and
  /// vocabularies from files in addition to parameters that set unset desired
  /// features (e.g: alignments, quality-scores).
  ///
  /// This is equivalent to a call to:
  /// ```cpp
  ///    Service(options, AlignedMemory(),  AlignedMemory())
  /// ```
  /// wherein empty memory is passed and internal flow defaults to file-based
  /// model, shortlist loading.
  explicit Service(Ptr<Options> options)
      : Service(options, AlignedMemory(), AlignedMemory()) {}

  /// Construct Service from a string configuration.
  /// @param [in] config string parsable as YAML expected to adhere with marian
  /// config
  /// @param [in] model_memory byte array (aligned to 256!!!) that contains the
  /// bytes of a model.bin. Optional.
  /// @param [in] shortlistMemory byte array of shortlist (aligned to 64)
  explicit Service(const std::string &config,
                   AlignedMemory modelMemory = AlignedMemory(),
                   AlignedMemory shortlistMemory = AlignedMemory())
      : Service(parseOptions(config, /*validate=*/false), std::move(modelMemory),
                std::move(shortlistMemory)) {}

  /// Explicit destructor to clean up after any threads initialized in
  /// asynchronous operation mode.
  ~Service();

  /// To stay efficient and to refer to the string for alignments, expects
  /// ownership be moved through `std::move(..)`
  ///
  ///  @param [in] source: rvalue reference of string to be translated.
  std::future<Response> translate(std::string &&source);

  /// Translate an input, providing Options to construct Response. This is
  /// useful when one has to set/unset alignments or quality in the Response to
  /// save compute spent in constructing these objects.
  ///
  /// @param [in] source: rvalue reference of the string to be translated
  /// @param [in] responseOptions: Options indicating whether or not to include
  /// some member in the Response, also specify any additional configurable
  /// parameters.
  std::future<Response> translate(std::string &&source,
                                  ResponseOptions options);

  /// Translate an input, providing TranslationRequest across all texts to
  /// construct Response. Provides the browser with the ability to break texts
  /// into multiple Request keeping gains from efficiently batching internally.
  /// Also useful when one has to set/unset alignments or quality in the
  /// Response to save compute spent in constructing these objects.

  /// @param [in] source: rvalue reference of the string to be translated
  /// @param [in] translationRequest: TranslationRequest (Unified API)
  /// indicating whether or not to include some member in the Response, also
  /// specify any additional configurable parameters.

  std::vector<Response>
  translateMultiple(std::vector<std::string> &&source,
                    TranslationRequest translationRequest);

  /// Returns if model is alignment capable or not. Exists for not breaking
  /// things at the extension.
  bool isAlignmentSupported() const {
    return options_->hasAndNotEmpty("alignment");
  }

private:
  /// Queue an input for translation.
  std::future<Response> queueRequest(std::string &&input,
                                     ResponseOptions responseOptions);

  /// Dispatch call to translate after inserting in queue
  void dispatchTranslate();

  /// Build numTranslators number of translators with options from options
  void build_translators(Ptr<Options> options, size_t numTranslators);
  /// Initializes a blocking translator without using std::thread
  void initialize_blocking_translator();
  /// Translates through direct interaction between batcher_ and translators_
  void blocking_translate();

  /// Launches multiple workers of translators using std::thread
  /// Reduces to ABORT if called when not compiled WITH_PTHREAD
  void initialize_async_translators();
  /// Async translate produces to a producer-consumer queue as batches are
  /// generated by Batcher. In another thread, the translators consume from
  /// producer-consumer queue.
  /// Reduces to ABORT if called when not compiled WITH_PTHREAD
  void async_translate();

  /// Number of workers to launch.
  size_t numWorkers_; // ORDER DEPENDENCY (pcqueue_)

  /// Options object holding the options Service was instantiated with.
  Ptr<Options> options_;

  /// Model memory to load model passed as bytes.
  AlignedMemory modelMemory_; // ORDER DEPENDENCY (translators_)
  /// Shortlist memory passed as bytes.
  AlignedMemory shortlistMemory_; // ORDER DEPENDENCY (translators_)

  /// Holds instances of batch translators, just one in case
  /// of single-threaded application, numWorkers_ in case of multithreaded
  /// setting.
  std::vector<BatchTranslator>
      translators_; // ORDER DEPENDENCY (modelMemory_, shortlistMemory_)

  /// Stores requestId of active request. Used to establish
  /// ordering among requests and logging/book-keeping.

  size_t requestId_;

  /// Store vocabs representing source and target.
  std::vector<Ptr<Vocab const>> vocabs_; // ORDER DEPENDENCY (text_processor_)

  /// TextProcesser takes a blob of text and converts into format consumable by
  /// the batch-translator and annotates sentences and words.
  TextProcessor text_processor_; // ORDER DEPENDENCY (vocabs_)

  /// Batcher handles generation of batches from a request, subject to
  /// packing-efficiency and priority optimization heuristics.
  Batcher batcher_;

  // The following constructs are available providing full capabilities on a non
  // WASM platform, where one does not have to hide threads.
#ifndef WASM_COMPATIBLE_SOURCE
  PCQueue<Batch> pcqueue_; // ORDER DEPENDENCY (numWorkers_)
  std::vector<std::thread> workers_;
#endif // WASM_COMPATIBLE_SOURCE
};

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_SERVICE_H_
