
#include <random>
#include <thread>

#include "catch.hpp"
#include "translator/cache.h"
#include "translator/history.h"

using namespace marian::bergamot;

TEST_CASE("Test Cache in a threaded setting") {
  size_t numThreads = 100;
  size_t numIters = 10000;
  using TestCache = AtomicCache<int, int>;

  TestCache cache(300);

  // Insert (x, x) into cache always. Tries to fetch, if hit assert the value is (x, x).
  // Not sure if this catches any ABA problem.

  auto op = [numIters, &cache]() {
    std::mt19937_64 randomGenerator;
    for (size_t i = 0; i < numIters; i++) {
      int key = randomGenerator();
      TestCache::FloatingRecord record = cache.Find(key);
      if (record) {
        auto [key, value] = *record;
        assert(value == key);
      }

      TestCache::FloatingRecord replacement = std::make_shared<TestCache::Record>(key, key);
      cache.Store(replacement);
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
  using TranslationCache = AtomicCache<size_t, marian::History>;
  TranslationCache translationCache(100);
}
