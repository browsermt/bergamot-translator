#include "service.h"
#include "batch.h"
#include "definitions.h"

#include <string>
#include <utility>

namespace marian {
namespace bergamot {

Service::Service(Ptr<Options> options)
    : ServiceBase(options), numWorkers_(options->get<int>("cpu-threads")),
      pcqueue_(numWorkers_),
      capacityBytes_(options->get<int>("capacity-bytes")) {
  if (numWorkers_ == 0) {
    ABORT("Fatal: Attempt to create multithreaded instance with --cpu-threads "
          "0. ");
  }

  translators_.reserve(numWorkers_);
  workers_.reserve(numWorkers_);

  for (size_t cpuId = 0; cpuId < numWorkers_; cpuId++) {
    marian::DeviceId deviceId(cpuId, DeviceType::cpu);
    translators_.emplace_back(deviceId, vocabs_, options);
    auto &translator = translators_.back();

    workers_.emplace_back([&translator, this] {
      translator.initialize();

      // Run thread mainloop
      Batch batch;
      Histories histories;
      while (true) {
        pcqueue_.ConsumeSwap(batch);
        if (batch.isPoison()) {
          return;
        } else {
          translator.translate(batch);
        }
      }
    });
  }
}

std::future<Response> Service::translate(std::string &&input) {
  // Takes in a blob of text. Segments and SentenceRanges are
  // extracted from the input (blob of text) and used to construct a Request
  // along with a promise. promise value is set by the worker completing a
  // request.
  //
  // Batcher, which currently runs on the main thread constructs batches out
  // of a single request (at the moment) and adds them into a
  // Producer-Consumer queue holding a bunch of requestSentences used to
  // construct batches.
  // TODO(jerin): Make asynchronous and compile from multiple requests.
  //
  // returns future corresponding to the promise.
  Ptr<RequestTracker> tracker =
      std::move(translatePart(std::move(input), /*lineNumberBegin=*/0));
  std::future<Response> future = std::move(tracker->future);
  return future;
}

Ptr<RequestTracker> Service::translatePart(std::string &&input,
                                           int lineNumberBegin) {

  Ptr<RequestTracker> tracker = New<RequestTracker>();
  std::promise<Response> responsePromise;
  auto future = responsePromise.get_future();
  tracker->set_future(std::move(future));

  size_t inputBytes = input.size();

  if (inputBytes > capacityBytes_) {
    // Check if input exceeds capacity, reject if this is the case.
    tracker->setStatus(StatusCode::REJECTED_MEMORY);
    Response emptyResponse = Response::EmptyResponse();
    responsePromise.set_value(std::move(emptyResponse));
  } else {

    // Accept request in, and adjust capacity accordingly.
    capacityBytes_ -= input.size();
    LOG(info, "CapacityBytes {}", capacityBytes_.load());

    // A prepareRequest lambda allows this segment to be executed
    // synchronously or asynchronously, both of which are done below.
    auto prepareRequest = [&]() {
      Segments segments;
      SentenceRanges sourceRanges;
      text_processor_.process(input, segments, sourceRanges);

      Ptr<Request> request =
          New<Request>(requestId_++, lineNumberBegin, /*nice=*/20, vocabs_,
                       std::move(input), std::move(segments),
                       std::move(sourceRanges), std::move(responsePromise));

      // Set tracker to track request. Set tracker on request so tracker is
      // updated when request is complete.
      tracker->track(request);

      auto callback = [tracker, this, inputBytes]() {
        tracker->setStatus(StatusCode::SUCCESS);
        capacityBytes_ += inputBytes;
        LOG(info, "CapacityBytes {}", capacityBytes_.load());
      };

      request->onCompleteRequest(std::move(callback));

      batcher_.addWholeRequest(request);
      tracker->setStatus(StatusCode::QUEUED);
    };

    // If there are more than 1 workers, we're operating in async
    // multithread setting. The preprocessing and queue calls can also be
    // done in the background.
    auto queueInBackground = [&]() {
      prepareRequest();
      enqueue();
    };
    std::async(std::launch::async, queueInBackground);
  }

  return tracker;
}

void Service::cancel(RequestTracker *requestTracker) {
  std::async([&]() { batcher_.cancel(requestTracker); });
}

void Service::amend(RequestTracker *requestTracker, int nice) {
  std::async([&]() { batcher_.amend(requestTracker, nice); });
}

void Service::enqueue() {
  Batch batch;
  while (batcher_ >> batch) {
    pcqueue_.ProduceSwap(batch);
  }
}

void Service::stop() {
  for (auto &worker : workers_) {
    Batch poison = Batch::poison();
    pcqueue_.ProduceSwap(poison);
  }

  for (auto &worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }

  workers_.clear();
}

Service::~Service() { stop(); }

} // namespace bergamot
} // namespace marian
