#ifndef SRC_BERGAMOT_SERVICE_H_
#define SRC_BERGAMOT_SERVICE_H_

#include "batch_translator.h"
#include "batcher.h"
#include "data/types.h"
#include "pcqueue.h"
#include "response.h"
#include "service_base.h"
#include "text_processor.h"

#include <queue>
#include <vector>

namespace marian {
namespace bergamot {

class Service : public ServiceBase {

  // Service exposes methods to translate an incoming blob of text to the
  // Consumer of bergamot API.
  //
  // An example use of this API looks as follows:
  //
  //  options = ...;
  //  service = Service(options);
  //  std::string input_blob = "Hello World";
  //  std::future<Response>
  //      response = service.translate(std::move(input_blob));
  //  response.wait();
  //  Response result = response.get();

public:
  explicit Service(Ptr<Options> options);
  void enqueue() override;
  void stop() override;
  ~Service();

private:
  size_t numWorkers_;      // ORDER DEPENDENCY
  PCQueue<Batch> pcqueue_; // ORDER DEPENDENCY
  std::vector<std::thread> workers_;
  std::vector<BatchTranslator> translators_;
};

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_SERVICE_H_
