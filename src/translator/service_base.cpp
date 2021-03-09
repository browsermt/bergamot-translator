#include "service_base.h"

namespace marian {
namespace bergamot {

ServiceBase::ServiceBase(Ptr<Options> options, const void * /*model_memory*/)
    : requestId_(0), vocabs_(std::move(loadVocabularies(options))),
      text_processor_(vocabs_, options), batcher_(options) {}

std::future<Response> ServiceBase::translate(std::string &&input) {
  Segments segments;
  SentenceRanges sourceRanges;
  text_processor_.process(input, segments, sourceRanges);

  std::promise<Response> responsePromise;
  auto future = responsePromise.get_future();

  Ptr<Request> request = New<Request>(
      requestId_++, /* lineNumberBegin = */ 0, vocabs_, std::move(input),
      std::move(segments), std::move(sourceRanges), std::move(responsePromise));

  batcher_.addWholeRequest(request);
  enqueue();
  return future;
}

NonThreadedService::NonThreadedService(Ptr<Options> options, const void * model_memory)
    : ServiceBase(options, model_memory),
      translator_(DeviceId(0, DeviceType::cpu), vocabs_, options, model_memory) {
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
