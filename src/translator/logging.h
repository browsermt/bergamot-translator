#include "3rd_party/marian-dev/src/3rd_party/spdlog/spdlog.h"
#include "common/logging.h"

namespace marian {
namespace bergamot {

// RAII Wrap around logging, to clean up after the object on stack.
class Logger {
 public:
  Logger() : marianLoggers_(createLoggers()) {
    // We are manually creating loggers, because this is usually created in marian as a side-effect of
    // config-parsing.
  }

  ~Logger() {
    // We need to manually destroy the loggers, as marian doesn't do
    // that but will complain when a new marian::Config tries to
    // initialise loggers with the same name.
    for (auto &logger : marianLoggers_) {
      if (logger) {
        spdlog::drop(logger->name());
      }
    }
  }

  // Explicit destructor above is an indicator we should not allow this class to copy-construct.
  Logger &operator=(const Logger &) = delete;
  Logger(const Logger &) = delete;

 private:
  using MarianLogger = std::shared_ptr<spdlog::logger>;
  std::vector<MarianLogger> marianLoggers_;
};
}  // namespace bergamot
}  // namespace marian
