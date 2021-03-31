#include "service.h"
#include "batch.h"
#include "definitions.h"

#include <string>
#include <utility>

inline std::vector<marian::Ptr<const marian::Vocab>>
loadVocabularies(marian::Ptr<marian::Options> options) {
  // @TODO: parallelize vocab loading for faster startup
  auto vfiles = options->get<std::vector<std::string>>("vocabs");
  // with the current setup, we need at least two vocabs: src and trg
  ABORT_IF(vfiles.size() < 2, "Insufficient number of vocabularies.");
  std::vector<marian::Ptr<marian::Vocab const>> vocabs(vfiles.size());
  std::unordered_map<std::string, marian::Ptr<marian::Vocab>> vmap;
  for (size_t i = 0; i < vocabs.size(); ++i) {
    auto m =
        vmap.emplace(std::make_pair(vfiles[i], marian::Ptr<marian::Vocab>()));
    if (m.second) { // new: load the vocab
      m.first->second = marian::New<marian::Vocab>(options, i);
      m.first->second->load(vfiles[i]);
    }
    vocabs[i] = m.first->second;
  }
  return vocabs;
}

namespace marian {
namespace bergamot {

Service::Service(Ptr<Options> options, const MemoryGift* modelMemory, const MemoryGift* shortlistMemory)
    : requestId_(0), vocabs_(std::move(loadVocabularies(options))),
      text_processor_(vocabs_, options), batcher_(options),
      numWorkers_(options->get<int>("cpu-threads")),
      modelMemory_(modelMemory), shortlistMemory_(shortlistMemory)
#ifndef WASM
      // 0 elements in PCQueue is illegal and can lead to failures. Adding a
      // guard to have at least one entry allocated. In the single-threaded
      // case, while initialized pcqueue_ remains unused.
      ,
      pcqueue_(std::max<size_t>(1, numWorkers_))
#endif
{

  if (numWorkers_ == 0) {
    build_translators(options, /*numTranslators=*/1);
    initialize_blocking_translator();
  } else {
    build_translators(options, numWorkers_);
    initialize_async_translators();
  }
}

void Service::build_translators(Ptr<Options> options, size_t numTranslators) {
  translators_.reserve(numTranslators);
  for (size_t cpuId = 0; cpuId < numTranslators; cpuId++) {
    marian::DeviceId deviceId(cpuId, DeviceType::cpu);
    translators_.emplace_back(deviceId, vocabs_, options, modelMemory_, shortlistMemory_);
  }
}

void Service::initialize_blocking_translator() {
  translators_.back().initialize();
}

void Service::blocking_translate() {
  Batch batch;
  while (batcher_ >> batch) {
    auto &translator = translators_.back();
    translator.translate(batch);
  }
}

#ifndef WASM
void Service::initialize_async_translators() {
  workers_.reserve(numWorkers_);

  for (size_t cpuId = 0; cpuId < numWorkers_; cpuId++) {
    auto &translator = translators_[cpuId];
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

void Service::async_translate() {
  Batch batch;
  while (batcher_ >> batch) {
    pcqueue_.ProduceSwap(batch);
  }
}
#else  // WASM
void Service::initialize_async_translators() {
  ABORT("Cannot run in async mode without multithreading.");
}

void Service::async_translate() {
  ABORT("Cannot run in async mode without multithreading.");
}
#endif // WASM

std::future<Response> Service::translate(std::string &&input) {
  Segments segments;
  AnnotatedText source(std::move(input));
  text_processor_.process(source, segments);

  std::promise<Response> responsePromise;
  auto future = responsePromise.get_future();

  Ptr<Request> request = New<Request>(
      requestId_++, /* lineNumberBegin = */ 0, vocabs_, std::move(source),
      std::move(segments), std::move(responsePromise));

  batcher_.addWholeRequest(request);
  if (numWorkers_ == 0) {
    blocking_translate();
  } else {
    async_translate();
  }
  return future;
}

Service::~Service() {
#ifndef WASM
  for (size_t workerId = 0; workerId < numWorkers_; workerId++) {

    Batch poison = Batch::poison();
    pcqueue_.ProduceSwap(poison);
  }

  for (size_t workerId = 0; workerId < numWorkers_; workerId++) {
    if (workers_[workerId].joinable()) {
      workers_[workerId].join();
    }
  }
#endif
}

} // namespace bergamot
} // namespace marian
