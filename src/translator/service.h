#ifndef SRC_BERGAMOT_SERVICE_H_
#define SRC_BERGAMOT_SERVICE_H_

#include "batch_translator.h"
#include "batcher.h"
#include "pcqueue.h"
#include "textops.h"
#include "translation_result.h"

#include <queue>
#include <vector>

#include "data/types.h"

namespace marian {
namespace bergamot {

class Service {

  // Service exposes methods to translate an incoming blob of text to the
  // Consumer of bergamot API.
  //
  // An example use of this API looks as follows:
  //
  //  options = ...;
  //  service = Service(options);
  //  std::string input_blob = "Hello World";
  //  std::future<TranslationResult>
  //      response = service.translate(std::move(input)_blob);
  //  response.wait();
  //  TranslationResult result = response.get();

public:
  explicit Service(Ptr<Options> options);
  std::future<TranslationResult> translateWithCopy(std::string input);
  std::future<TranslationResult> translate(std::string &&input);
  void stop();
  Ptr<Vocab const> sourceVocab() const { return vocabs_.front(); }
  Ptr<Vocab const> targetVocab() const { return vocabs_.back(); }
  ~Service();

private:
  unsigned int requestId_;
  unsigned int batchNumber_;
  int numWorkers_;

  // Consists of:
  // 1. an instance of text-processing class (TextProcessor),
  // 2. a Batcher // class which handles efficient batching by minimizing
  //    padding wasting compute.
  // 3. Multiple workers - which are instances of BatchTranslators are
  //    spawned in threads. The Batcher acts as a producer for a
  //    producer-consumer queue, with idle BatchTranslators requesting batches
  //    as they're ready.

  std::vector<Ptr<Vocab const>> vocabs_;
  TextProcessor text_processor_;
  Batcher batcher_;
  PCQueue<PCItem> pcqueue_;
  std::vector<BatchTranslator> workers_;
};

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_SERVICE_H_
