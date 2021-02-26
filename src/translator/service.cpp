#include "service.h"
#include "batch.h"
#include "definitions.h"

#include <string>
#include <utility>

namespace marian {
namespace bergamot {

Service::Service(Ptr<Options> options)
    : ServiceBase(options), numWorkers_(options->get<int>("cpu-threads")),
      pcqueue_(numWorkers_) {
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
