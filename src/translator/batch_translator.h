#ifndef SRC_BERGAMOT_BATCH_TRANSLATOR_H_
#define SRC_BERGAMOT_BATCH_TRANSLATOR_H_

#include <string>
#include <vector>

#include "common/utils.h"
#include "data/shortlist.h"
#include "definitions.h"
#include "pcqueue.h"
#include "request.h"
#include "translator/history.h"
#include "translator/scorers.h"

namespace marian {
namespace bergamot {

class BatchTranslator {
  // Launches minimal marian-translation (only CPU at the moment) in individual
  // threads. Constructor launches each worker thread running mainloop().
  // mainloop runs until until it receives poison from the PCQueue. Threads are
  // shut down in Service which calls join() on the threads.

public:
  BatchTranslator(DeviceId const device, PCQueue<PCItem> &pcqueue,
                  Ptr<Options> options);
  void join();

  // convenience function for logging. TODO(jerin)
  std::string _identifier() { return "worker" + std::to_string(device_.no); }

private:
  void initGraph();
  void translate(RequestSentences &requestSentences, Histories &histories);
  void mainloop();

  Ptr<Options> options_;

  DeviceId device_;
  std::vector<Ptr<Vocab const>> vocabs_;
  Ptr<ExpressionGraph> graph_;
  std::vector<Ptr<Scorer>> scorers_;
  Ptr<data::ShortlistGenerator const> slgen_;

  PCQueue<PCItem> *pcqueue_;
  std::thread thread_;
};
} // namespace bergamot
} // namespace marian

#endif //  SRC_BERGAMOT_BATCH_TRANSLATOR_H_
