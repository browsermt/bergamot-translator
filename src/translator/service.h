#ifndef SRC_BERGAMOT_SERVICE_H_
#define SRC_BERGAMOT_SERVICE_H_

#include "batch_translator.h"
#include "batcher.h"
#include "data/types.h"
#include "response.h"
#include "text_processor.h"
#include "translator/parser.h"

#ifdef WITH_PTHREADS
#include "pcqueue.h"
#endif

#include <queue>
#include <vector>

namespace marian {
namespace bergamot {

/// Service exposes methods to translate an incoming blob of text to the
/// Consumer of bergamot API.
///
/// An example use of this API looks as follows:
///
///  options = ...;
///  service = Service(options);
///  std::string input_blob = "Hello World";
///  std::future<Response>
///      response = service.translate(std::move(input_blob));
///  response.wait();
///  Response result = response.get();
class Service {

public:
  /// @param options Marian options object
  /// @param model_memory byte array (aligned to 64!!!) that contains the bytes
  /// of a model.bin. Optional, defaults to nullptr when not used
  explicit Service(Ptr<Options> options, const void *model_memory = nullptr);

  /// Construct Service from a string configuration.
  /// @param [in] config string parsable as YAML expected to adhere with marian
  /// config
  explicit Service(const std::string &config,
                   const void *model_memory = nullptr)
      : Service(parseOptions(config), model_memory) {}

  /// Shared pointer to source-vocabulary.
  Ptr<Vocab const> sourceVocab() const { return vocabs_.front(); }

  /// Shared pointer to target vocabulary.
  Ptr<Vocab const> targetVocab() const { return vocabs_.back(); }

  /// To stay efficient and to refer to the string for alignments, expects the
  /// ownership be moved through std::move(..)
  ///
  ///  @param [in] rvalue of string to be translated.
  std::future<Response> translate(std::string &&input);

  ~Service();

private:
  /// Launches a blocking translator without using std::thread
  void initialize_blocking_translator(Ptr<Options> options);
  /// Translates through direct interaction between batcher_ and translators_
  void blocking_translate();

  /// Launches multiple workers of translators using std::thread
  void initialize_async_translators(Ptr<Options> options);
  void async_translate();

  /// Number of workers to launch.
  size_t numWorkers_; // ORDER DEPENDENCY (pcqueue_)
  const void *model_memory_;
  std::vector<BatchTranslator> translators_;

  size_t requestId_;
  std::vector<Ptr<Vocab const>> vocabs_; // ORDER DEPENDENCY (text_processor_)
  TextProcessor text_processor_;         // ORDER DEPENDENCY (vocabs_)
  Batcher batcher_;

  // The following constructs are required only if PTHREADS are enabled to
  // provide WASM with a substandard-service.
#ifdef WITH_PTHREADS
  PCQueue<Batch> pcqueue_; // ORDER DEPENDENCY (numWorkers_)
  std::vector<std::thread> workers_;
#endif // WITH_PTHREADS
};

inline std::vector<Ptr<const Vocab>> loadVocabularies(Ptr<Options> options) {
  // @TODO: parallelize vocab loading for faster startup
  auto vfiles = options->get<std::vector<std::string>>("vocabs");
  // with the current setup, we need at least two vocabs: src and trg
  ABORT_IF(vfiles.size() < 2, "Insufficient number of vocabularies.");
  std::vector<Ptr<Vocab const>> vocabs(vfiles.size());
  std::unordered_map<std::string, Ptr<Vocab>> vmap;
  for (size_t i = 0; i < vocabs.size(); ++i) {
    auto m = vmap.emplace(std::make_pair(vfiles[i], Ptr<Vocab>()));
    if (m.second) { // new: load the vocab
      m.first->second = New<Vocab>(options, i);
      m.first->second->load(vfiles[i]);
    }
    vocabs[i] = m.first->second;
  }
  return vocabs;
}

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_SERVICE_H_
