#include "3rd_party/marian-dev/src/3rd_party/spdlog/spdlog.h"
#include "common/logging.h"

namespace marian {
namespace bergamot {

// RAII Wrap around logging, to clean up after the object on stack.
class Logger {
 public:
  struct Config {
    std::string level{"off"};
    template <class App>
    static void addOptions(App &app, Config &config) {
      app.add_option("--log-level", config.level,
                     "Set verbosity level of logging: trace, debug, info, warn, err(or), critical, off");
    }
  };

  Logger(const Config &config) : marianLoggers_(createLoggers()) {
    // We are manually creating loggers, because this is usually created in marian as a side-effect of
    // config-parsing.
    for (auto &logger : marianLoggers_) {
      setLoggingLevel(*logger, config.level);
    }
  }

  // Taken from
  // https://github.com/marian-nmt/marian-dev/blob/c84599d08ad69059279abd5a7417a8053db8b631/src/common/logging.cpp#L45
  static bool setLoggingLevel(spdlog::logger &logger, std::string const level) {
    if (level == "trace")
      logger.set_level(spdlog::level::trace);
    else if (level == "debug")
      logger.set_level(spdlog::level::debug);
    else if (level == "info")
      logger.set_level(spdlog::level::info);
    else if (level == "warn")
      logger.set_level(spdlog::level::warn);
    else if (level == "err" || level == "error")
      logger.set_level(spdlog::level::err);
    else if (level == "critical")
      logger.set_level(spdlog::level::critical);
    else if (level == "off")
      logger.set_level(spdlog::level::off);
    else {
      logger.warn("Unknown log level '{}' for logger '{}'", level.c_str(), logger.name().c_str());
      return false;
    }
    return true;
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
