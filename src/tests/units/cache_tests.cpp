
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
    randomGenerator.seed(42);  // reproducible outputs
    Value randMax = 2000;

    for (size_t i = 0; i < numIters; i++) {
      Key query = randomGenerator() % randMax;
      std::pair<bool, Value> result = cache.find(query);
      if (result.first) {
        REQUIRE(result.second == query);
      }

      Value value = query;
      cache.store(/*key=*/query, std::move(value));
    }
  };

  std::vector<std::thread> workers;
  for (size_t t = 0; t < numThreads; t++) {
    workers.emplace_back(op);
  }

  for (size_t t = 0; t < numThreads; t++) {
    workers[t].join();
  }

  TestCache::Stats stats = cache.stats();
  float hitRate = static_cast<float>(stats.hits) / static_cast<float>(stats.hits + stats.misses);

  // This is non-deterministic due to threads.
  std::cout << "Hit-Rate:" << hitRate << "\n";
  std::cout << "(Hits, Misses) = " << stats.hits << " " << stats.misses << "\n";

  // Can we create a specialization of the actual cache-type we want? Does it compile, at least?
  // We already have Ptr<History>, it's easier to move Ptr<History> to cache.
  TranslationCache translationCache(/*size=*/300, /*mutexBuckets=*/16);
}
