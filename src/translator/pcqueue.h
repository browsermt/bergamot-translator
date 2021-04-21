#ifndef SRC_BERGAMOT_PCQUEUE_H_
#define SRC_BERGAMOT_PCQUEUE_H_

#include "common/logging.h"

#include <algorithm>
#include <cerrno>
#include <iostream>
#include <memory>
#include <mutex>

#ifdef __APPLE__
#include <mach/semaphore.h>
#include <mach/task.h>
#include <mach/mach_traps.h>
#include <mach/mach.h>
#elif defined(__linux)
#include <semaphore.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#endif

#if __GNUC__ >= 3
#define UTIL_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define UTIL_UNLIKELY(x) (x)
#endif

namespace marian {
namespace bergamot {

/* OS X Maverick and Boost interprocess were doing "Function not implemented."
 * So this is my own wrapper around the mach kernel APIs.
 */
#ifdef __APPLE__

class Semaphore {
  public:
    explicit Semaphore(int value) : task_(mach_task_self()) {
      ABORT_IF(KERN_SUCCESS != semaphore_create(task_, &back_, SYNC_POLICY_FIFO, value), "Could not create semaphore");
    }

    ~Semaphore() {
      if (KERN_SUCCESS != semaphore_destroy(task_, back_)) {
        std::cerr << "Could not destroy semaphore" << std::endl;
        abort();
      }
    }

    void wait() {
      ABORT_IF(KERN_SUCCESS != semaphore_wait(back_), "Wait for semaphore failed");
    }

    void post() {
      ABORT_IF(KERN_SUCCESS != semaphore_signal(back_), "Could not post to semaphore");
    }

  private:
    semaphore_t back_;
    task_t task_;
};

inline void WaitSemaphore(Semaphore &semaphore) {
  semaphore.wait();
}

#elif defined(__linux)

class Semaphore {
  public:
    explicit Semaphore(unsigned int value) {
      ABORT_IF(sem_init(&sem_, 0, value), "Could not create semaphore");
    }

    ~Semaphore() {
      if (-1 == sem_destroy(&sem_)) {
        std::cerr << "Could not destroy semaphore" << std::endl;
        abort();
      }
    }

    void wait() {
      while (-1 == sem_wait(&sem_)) {
        ABORT_IF(errno != EINTR, "Wait for semaphore failed");
      }
    }

    void post() {
      ABORT_IF(-1 == sem_post(&sem_), "Could not post to semaphore");
    }

  private:
    sem_t sem_;
};

inline void WaitSemaphore(Semaphore &semaphore) {
  semaphore.wait();
}

#elif defined(_WIN32) || defined(_WIN64)

class Semaphore {
  public:
    explicit Semaphore(LONG value) : sem_(CreateSemaphoreA(NULL, value, 2147483647, NULL)) {
      ABORT_IF(!sem_, "Could not CreateSemaphore {}", GetLastError());
    }

    ~Semaphore() {
      CloseHandle(sem_);
    }


    void wait() {
      while (true) {
        switch (WaitForSingleObject(sem_, 0L)) {
          case WAIT_OBJECT_0:
            return;
          case WAIT_ABANDONED:
            ABORT("A semaphore can't be abandoned, confused by Windows");
          case WAIT_TIMEOUT:
            continue;
          case WAIT_FAILED:
            ABORT("Waiting on Semaphore failed {}", GetLastError());
        }
      }
    }

    void post() {
      ABORT_IF(!ReleaseSemaphore(sem_, 1, NULL), "Failed to release Semaphore {}", GetLastError());
    }

  private:
    HANDLE sem_;
};

inline void WaitSemaphore(Semaphore &semaphore) {
  semaphore.wait();
}

#else
typedef boost::interprocess::interprocess_semaphore Semaphore;

inline void WaitSemaphore (Semaphore &on) {
  while (1) {
    try {
      on.wait();
      break;
    } catch (boost::interprocess::interprocess_exception &e) {
      if (e.get_native_error() != EINTR) {
        throw;
      }
    }
  }
}

#endif // Cases for semaphore support

/**
 * Producer consumer queue safe for multiple producers and multiple consumers.
 * T must be default constructable and have operator=.
 * The value is copied twice for Consume(T &out) or three times for Consume(),
 * so larger objects should be passed via pointer.
 * Strong exception guarantee if operator= throws.  Undefined if semaphores
 * throw.
 */
template <class T> class PCQueue {
 public:
  explicit PCQueue(size_t size)
   : empty_(size), used_(0),
     storage_(new T[size]),
     end_(storage_.get() + size),
     produce_at_(storage_.get()),
     consume_at_(storage_.get()) {}

  // Add a value to the queue.
  void Produce(const T &val) {
    WaitSemaphore(empty_);
    {
      std::lock_guard<std::mutex> produce_lock(produce_at_mutex_);
      try {
        *produce_at_ = val;
      } catch (...) {
        empty_.post();
        throw;
      }
      if (++produce_at_ == end_) produce_at_ = storage_.get();
    }
    used_.post();
  }

  // Add a value to the queue, but swap it into place.
  void ProduceSwap(T &val) {
    WaitSemaphore(empty_);
    {
      std::lock_guard<std::mutex> produce_lock(produce_at_mutex_);
      try {
        std::swap(*produce_at_, val);
      } catch (...) {
        empty_.post();
        throw;
      }
      if (++produce_at_ == end_) produce_at_ = storage_.get();
    }
    used_.post();
  }


  // Consume a value, assigning it to out.
  T& Consume(T &out) {
    WaitSemaphore(used_);
    {
      std::lock_guard<std::mutex> consume_lock(consume_at_mutex_);
      try {
        out = *consume_at_;
      } catch (...) {
        used_.post();
        throw;
      }
      if (++consume_at_ == end_) consume_at_ = storage_.get();
    }
    empty_.post();
    return out;
  }

  // Consume a value, swapping it to out.
  T& ConsumeSwap(T &out) {
    WaitSemaphore(used_);
    {
      std::lock_guard<std::mutex> consume_lock(consume_at_mutex_);
      try {
        std::swap(out, *consume_at_);
      } catch (...) {
        used_.post();
        throw;
      }
      if (++consume_at_ == end_) consume_at_ = storage_.get();
    }
    empty_.post();
    return out;
  }


  // Convenience version of Consume that copies the value to return.
  // The other version is faster.
  T Consume() {
    T ret;
    Consume(ret);
    return ret;
  }

 private:
  // Number of empty spaces in storage_.
  Semaphore empty_;
  // Number of occupied spaces in storage_.
  Semaphore used_;

  std::unique_ptr<T[]> storage_;

  T *const end_;

  // Index for next write in storage_.
  T *produce_at_;
  std::mutex produce_at_mutex_;

  // Index for next read from storage_.
  T *consume_at_;
  std::mutex consume_at_mutex_;
};

template <class T> struct UnboundedPage {
  UnboundedPage() : next(nullptr) {}
  UnboundedPage *next;
  T entries[1023];
};

template <class T> class UnboundedSingleQueue {
  public:
    UnboundedSingleQueue() : valid_(0) {
      SetFilling(new UnboundedPage<T>());
      SetReading(filling_);
    }

    void Produce(T &&val) {
      if (filling_current_ == filling_end_) {
        UnboundedPage<T> *next = new UnboundedPage<T>();
        filling_->next = next;
        SetFilling(next);
      }
      *(filling_current_++) = std::move(val);
      valid_.post();
    }

    void Produce(const T &val) {
      Produce(T(val));
    }

    T& Consume(T &out) {
      WaitSemaphore(valid_);
      if (reading_current_ == reading_end_) {
        SetReading(reading_->next);
      }
      out = std::move(*(reading_current_++));
      return out;
    }

    // Warning: very much a no-guarantees race-condition-rich implementation!
    // But sufficient for our specific purpose: The single thread that consumes
    // is also the only one that checks Empty, and knows that it's racing.
    bool Empty() const {
      return reading_current_ == filling_current_;
    }

  private:
    void SetFilling(UnboundedPage<T> *to) {
      filling_ = to;
      filling_current_ = to->entries;
      filling_end_ = filling_current_ + sizeof(to->entries) / sizeof(T);
    }
    void SetReading(UnboundedPage<T> *to) {
      reading_.reset(to);
      reading_current_ = to->entries;
      reading_end_ = reading_current_ + sizeof(to->entries) / sizeof(T);
    }

    Semaphore valid_;

    UnboundedPage<T> *filling_;

    std::unique_ptr<UnboundedPage<T> > reading_;

    T *filling_current_;
    T *filling_end_;
    T *reading_current_;
    T *reading_end_;

    UnboundedSingleQueue(const UnboundedSingleQueue &) = delete;
    UnboundedSingleQueue &operator=(const UnboundedSingleQueue &) = delete;
};

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_PCQUEUE_H_
