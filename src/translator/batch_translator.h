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

#ifdef WITH_PTHREADS
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
  BatchTranslator(DeviceId const device, std::vector<Ptr<Vocab const>> &vocabs,
                  Ptr<Options> options);

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
};

} // namespace bergamot
} // namespace marian

#endif //  SRC_BERGAMOT_BATCH_TRANSLATOR_H_
