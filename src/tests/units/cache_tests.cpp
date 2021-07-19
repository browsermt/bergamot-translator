#include <random>
#include <vector>

#include "catch.hpp"
#include "translator/cache.h"

using marian::bergamot::LRUCache;

const int SIZE = 1000;

TEST_CASE("LRU Cache single-thread") {
  typedef LRUCache<int, int> Cache;

  int size = SIZE;
  Cache cache(size);

  auto identity = [](int key) { return key; };
  int val = -1;

  // First pass
  for (int key = 0; key < size; key++) {
    CHECK(!cache.fetch(key, val));  // cache-miss
    cache.insert(key, identity(key));
  }

  // Second pass
  for (int key = 0; key < size; key++) {
    CHECK(cache.fetch(key, val));  // cache-hit
    CHECK(val == identity(key));
  }

  // Add more
  for (int key = size; key < 2 * size; key++) {
    CHECK(!cache.fetch(key, val));  // cache-miss
    cache.insert(key, identity(key));
  }

  for (int key = 0; key < size; key++) {
    CHECK(!cache.fetch(key, val));  // cache-miss due to eviction
  }
}

TEST_CASE("LRU Cache multi-thread") {
  typedef LRUCache<int, int> Cache;

  int size = SIZE;
  Cache cache(size);

  // cache.debug();
  std::vector<std::thread> threads;

  auto f = [](int key) { return key; };
  int val = -1;
  auto fn = [&cache, &f](int key) { cache.insert(key, f(key)); };

  std::cout << threads.size() << std::endl;

  // First pass
  for (int key = 0; key < size; key++) {
    CHECK(!cache.fetch(key, val));  // cache-miss
    threads.emplace_back(fn, key);
  }

  std::cout << threads.size() << std::endl;

  // Barrier
  for (auto &worker : threads) {
    worker.join();
  }

  threads.clear();
  // cache.debug();

  // Second pass
  for (int key = 0; key < size; key++) {
    if (cache.fetch(key, val)) {  // cache-hit, but threads mean ordering can lead to weird evictions.
      CHECK(val == f(key));
    }
  }

  // Add more
  for (int key = size; key < 2 * size; key++) {
    CHECK(!cache.fetch(key, val));  // cache-miss
    cache.insert(key, f(key));
  }

  for (int key = 0; key < size; key++) {
    CHECK(!cache.fetch(key, val));  // cache-miss due to eviction
  }
}
