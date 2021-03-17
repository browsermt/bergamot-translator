#include "service_base.h"

namespace marian {
namespace bergamot {

ServiceBase::ServiceBase(Ptr<Options> options)
    : requestId_(0), vocabs_(std::move(loadVocabularies(options))),
      text_processor_(vocabs_, options), batcher_(options) {}

std::future<Response> ServiceBase::translate(std::string &&input) {
  Segments segments;
  AnnotatedBlob source(std::move(input));
  text_processor_.process(source, segments);

  std::promise<Response> responsePromise;
  auto future = responsePromise.get_future();

  Ptr<Request> request = New<Request>(
      requestId_++, /* lineNumberBegin = */ 0, vocabs_, std::move(source),
      std::move(segments), std::move(responsePromise));

  batcher_.addWholeRequest(request);
  enqueue();
  return future;
}

NonThreadedService::NonThreadedService(Ptr<Options> options)
    : ServiceBase(options),
      translator_(DeviceId(0, DeviceType::cpu), vocabs_, options) {
  translator_.initialize();
}

void NonThreadedService::enqueue() {
  // Queue single-threaded
  Batch batch;
  while (batcher_ >> batch) {
    translator_.translate(batch);
  }
}

} // namespace bergamot
} // namespace marian
