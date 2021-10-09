
#include <atomic>
#include <random>
#include <thread>

#include "catch.hpp"
#include "translator/cache.h"
#include "translator/history.h"

using namespace marian::bergamot;

TEST_CASE("Test Cache in a threaded setting") {
  size_t numThreads = 100;
  size_t numIters = 10000;

  struct Entry {
    int offset;
    int value;
  };

  using TestCache = AtomicCache<int, Entry>;

  TestCache cache(300);

  // Insert (x, x) into cache always. Tries to fetch, if hit assert the value is (x, x).
  // Not sure if this catches any ABA problem.

  auto op = [numIters, &cache](int offset) {
    std::mt19937_64 randomGenerator;
    for (size_t i = 0; i < numIters; i++) {
      int key = randomGenerator();
      Entry entry;

      if (cache.Find(key, entry)) {
        assert(entry.value == entry.offset + key);
      }

      TestCache::Record replacement;
      replacement.key = key;
      replacement.value = Entry{offset, offset + key};
      cache.Store(replacement);
    }
  };

  std::vector<std::thread> workers;
  for (size_t t = 0; t < numThreads; t++) {
    workers.emplace_back(op, t);
  }

  for (size_t t = 0; t < numThreads; t++) {
    workers[t].join();
  }

  // Can we create one for our use case, at compile?
  // AtomicCache<size_t, std::shared_ptr<marian::History>> TranslationCache(100);
}
