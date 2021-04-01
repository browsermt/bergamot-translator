#ifndef SRC_BERGAMOT_BATCH_TRANSLATOR_H_
#define SRC_BERGAMOT_BATCH_TRANSLATOR_H_

#include <string>
#include <vector>

#include "batch.h"
#include "common/utils.h"
#include "data/shortlist.h"
#include "definitions.h"
#include "request.h"
#include "translator/history.h"
#include "translator/scorers.h"

#ifndef WASM_COMPATIBLE_SOURCE
#include "pcqueue.h"
#endif

namespace marian {
namespace bergamot {

class BatchTranslator {
  // Launches minimal marian-translation (only CPU at the moment) in individual
  // threads. Constructor launches each worker thread running mainloop().
  // mainloop runs until until it receives poison from the PCQueue. Threads are
  // shut down in Service which calls join() on the threads.

public:
  /**
   * Initialise the marian translator.
   * @param device DeviceId that performs translation. Could be CPU or GPU
   * @param vocabs Vector that contains ptrs to two vocabs
   * @param options Marian options object
   * @param modelMemory byte array (aligned to 256!!!) that contains the bytes of a model.bin. Provide a nullptr if not used.
   * @param shortlistMemory byte array of shortlist (aligned to 64)
   */
  explicit BatchTranslator(DeviceId const device, std::vector<Ptr<Vocab const>> &vocabs,
                  Ptr<Options> options, const AlignedMemory* modelMemory, const AlignedMemory* shortlistMemory);

  // convenience function for logging. TODO(jerin)
  std::string _identifier() { return "worker" + std::to_string(device_.no); }
  void translate(Batch &batch);
  void initialize();

private:
  Ptr<Options> options_;
  DeviceId device_;
  std::vector<Ptr<Vocab const>> *vocabs_;
  Ptr<ExpressionGraph> graph_;
  std::vector<Ptr<Scorer>> scorers_;
  Ptr<data::ShortlistGenerator const> slgen_;
  const AlignedMemory* modelMemory_{nullptr};
  const AlignedMemory* shortlistMemory_{nullptr};
};

} // namespace bergamot
} // namespace marian

#endif //  SRC_BERGAMOT_BATCH_TRANSLATOR_H_
