#ifndef SRC_BERGAMOT_SERVICE_H_
#define SRC_BERGAMOT_SERVICE_H_

#include "batch_translator.h"
#include "data/types.h"
#include "response.h"
#include "response_builder.h"
#include "text_processor.h"
#include "threadsafe_batcher.h"
#include "translator/parser.h"
#include "vocabs.h"

#ifndef WASM_COMPATIBLE_SOURCE
#include <thread>
#endif

#include <queue>
#include <vector>

namespace marian {
namespace bergamot {

/// Service offers methods create an asynchronous translation service that
/// translates a plain (without any markups and emojis)  UTF-8 encoded text.
/// This implementation supports translation from 1 source language to 1 target
/// language.
///
///  This is intended to be similar to the ones  provided for training or
///  decoding in ML pipelines with the following  additional capabilities:
///
///  1. Provision of a request -> response based translation flow unlike the
///  usual a line based translation or decoding provided in most ML frameworks.
///  2. Internal handling of normalization etc which changes source text to
///  provide to client translation meta-information like alignments consistent
///  with the unnormalized input text.
///  3. The API splits each text entry into sentences internally, which are then
///  translated independent of each other. The translated sentences are then
///  joined back together and returned in Response.
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
/// Optionally Service can be initialized by also passing bytearray memories
/// for purposes of efficiency (which defaults to empty and then reads from
/// file supplied through config).
///
class Service {
 public:
  /// Construct Service from Marian options. If memoryBundle is empty, Service is
  /// initialized from file-based loading. Otherwise, Service is initialized from
  /// the given bytearray memories.
  /// @param options Marian options object
  /// @param memoryBundle holds all byte-array memories. Can be a set/subset of
  /// model, shortlist, vocabs and ssplitPrefixFile bytes. Optional.
  explicit Service(Ptr<Options> options, MemoryBundle memoryBundle = {});

  /// Construct Service from a string configuration. If memoryBundle is empty, Service is
  /// initialized from file-based loading. Otherwise, Service is initialized from
  /// the given bytearray memories.
  /// @param [in] config string parsable as YAML expected to adhere with marian config
  /// @param [in] memoryBundle holds all byte-array memories. Can be a set/subset of
  /// model, shortlist, vocabs and ssplitPrefixFile bytes. Optional.
  explicit Service(const std::string &config, MemoryBundle memoryBundle = {})
      : Service(parseOptions(config, /*validate=*/false), std::move(memoryBundle)) {}

  /// Explicit destructor to clean up after any threads initialized in
  /// asynchronous operation mode.
  ~Service();

  /// Translate an input, providing Options to construct Response. This is
  /// useful when one has to set/unset alignments or quality in the Response to
  /// save compute spent in constructing these objects.
  ///
  /// @param [in] source: rvalue reference of the string to be translated
  /// @param [in] responseOptions: Options indicating whether or not to include
  /// some member in the Response, also specify any additional configurable
  /// parameters.
  std::future<Response> translate(std::string &&source, ResponseOptions options = ResponseOptions());

  /// Translate multiple text-blobs in a single *blocking* API call, providing
  /// ResponseOptions which applies across all text-blobs dictating how to
  /// construct Response. ResponseOptions can be used to enable/disable
  /// additional information like quality-scores, alignments etc.
  ///
  /// All texts are combined to efficiently construct batches together providing
  /// speedups compared to calling translate() indepdently on individual
  /// text-blob. Note that there will be minor differences in output when
  /// text-blobs are individually translated due to approximations but similar
  /// quality nonetheless. If you have async/multithread capabilities, it is
  /// recommended to work with futures and translate() API.
  ///
  /// @param [in] source: rvalue reference of the string to be translated
  /// @param [in] responseOptions: ResponseOptions indicating whether or not
  /// to include some member in the Response, also specify any additional
  /// configurable parameters.
  std::vector<Response> translateMultiple(std::vector<std::string> &&source, ResponseOptions responseOptions);

  /// Returns if model is alignment capable or not.
  bool isAlignmentSupported() const { return options_->hasAndNotEmpty("alignment"); }

 private:
  /// Queue an input for translation.
  std::future<Response> queueRequest(std::string &&input, ResponseOptions responseOptions);

  /// Dispatch call to translate after inserting in queue
  void dispatchTranslate();

  /// Translates through direct interaction between batcher_ and translators_
  void blockIfWASM();

  /// Number of workers to launch.
  size_t numWorkers_;

  /// Options object holding the options Service was instantiated with.
  Ptr<Options> options_;

  /// Model memory to load model passed as bytes.
  AlignedMemory modelMemory_;  // ORDER DEPENDENCY (translators_)
  /// Shortlist memory passed as bytes.
  AlignedMemory shortlistMemory_;  // ORDER DEPENDENCY (translators_)

  /// Stores requestId of active request. Used to establish
  /// ordering among requests and logging/book-keeping.

  size_t requestId_;
  /// Store vocabs representing source and target.
  Vocabs vocabs_;  // ORDER DEPENDENCY (text_processor_)

  /// TextProcesser takes a blob of text and converts into format consumable by
  /// the batch-translator and annotates sentences and words.
  TextProcessor text_processor_;  // ORDER DEPENDENCY (vocabs_)

  /// Batcher handles generation of batches from a request, subject to
  /// packing-efficiency and priority optimization heuristics.
  ThreadsafeBatcher batcher_;

  // The following constructs are available providing full capabilities on a non
  // WASM platform, where one does not have to hide threads.
#ifdef WASM_COMPATIBLE_SOURCE
  BatchTranslator blocking_translator_;  // ORDER DEPENDENCY (modelMemory_, shortlistMemory_)
#else
  std::vector<std::thread> workers_;
#endif  // WASM_COMPATIBLE_SOURCE
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_SERVICE_H_
