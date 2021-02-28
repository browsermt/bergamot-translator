#ifndef SRC_BERGAMOT_SERVICE_H_
#define SRC_BERGAMOT_SERVICE_H_

#include "batch_translator.h"
#include "batcher.h"
#include "data/types.h"
#include "pcqueue.h"
#include "request_tracker.h"
#include "response.h"
#include "service_base.h"
#include "text_processor.h"

#include <queue>
#include <vector>

namespace marian {
namespace bergamot {

class Service : public ServiceBase {

public:
  explicit Service(Ptr<Options> options);

  std::future<Response> translate(std::string &&input);
  // Fresh translate method with RequestTracker instead of future being
  // prepared.
  Ptr<RequestTracker> translatePart(std::string &&input, int lineNumberBegin);

  // A consumer of API with access to a requestTracker can cancel the request or
  // amend priority, through the following functions. (TODO(jerinphilip): stub,
  // improve).
  void cancel(RequestTracker *requestTracker);
  void amend(RequestTracker *requestTracker, int nice);
  void stop() override;
  ~Service();

private:
  // Implements enqueue and top through blocking methods.
  void enqueue() override;

  std::atomic<size_t> capacityBytes_;

  /// Number of workers (each a translator) to launch.
  size_t numWorkers_; // ORDER DEPENDENCY

  /// Producer-Consumer queue. batcher_ as producer, writes batches to pcqueue_.
  /// translators_ consume batches from pcqueue_.
  PCQueue<Batch> pcqueue_; // ORDER DEPENDENCY

  /// Holds the threads to destruct properly through join.
  std::vector<std::thread> workers_;

  /// numWorkers_ instances of BatchTranslators.
  std::vector<BatchTranslator> translators_;
};

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_SERVICE_H_
