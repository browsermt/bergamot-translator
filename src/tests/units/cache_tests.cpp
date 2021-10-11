
#include <random>
#include <thread>

#include "catch.hpp"
#include "translator/cache.h"
#include "translator/history.h"

using namespace marian::bergamot;

TEST_CASE("Test Cache in a threaded setting") {
  size_t numThreads = 100;
  size_t numIters = 10000;
  using Key = int;
  using Value = int;
  using TestCache = AtomicCache<Key, Value>;

  TestCache cache(/*size=*/300, /*mutexBuckets=*/16);

  auto op = [numIters, &cache]() {
    std::mt19937_64 randomGenerator;
    for (size_t i = 0; i < numIters; i++) {
      Key query = randomGenerator();
      std::pair<bool, Value> result = cache.Find(query);
      if (result.first) {
        assert(result.second == query);
      }

      Value value = query;
      cache.Store(/*key=*/query, std::move(value));
    }
  };

  std::vector<std::thread> workers;
  for (size_t t = 0; t < numThreads; t++) {
    workers.emplace_back(op);
  }

  for (size_t t = 0; t < numThreads; t++) {
    workers[t].join();
  }

  // Can we create a specialization of the actual cache-type we want? Does it compile, at least?
  // We already have Ptr<History>, it's easier to move Ptr<History> to cache.
  // This is typedef-ed in cache.h now.
  // using TranslationCache = AtomicCache<size_t, std::shared_ptr<marian::History>>;
  TranslationCache translationCache(/*size=*/300, /*mutexBuckets=*/16);
}
