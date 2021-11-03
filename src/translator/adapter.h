#include "response.h"          // for Response
#include "service.h"           // for AsyncService
#include "translation_model.h" // for TranslationModel
#include <cstddef> // for size_t
#include <future>
#include <memory> // for shared_ptr
#include <optional>

namespace go_bergamot {
class StaticTranslator {
public:
  using Model = std::shared_ptr<marian::bergamot::TranslationModel>;
  using Service = marian::bergamot::AsyncService;
  using Response = marian::bergamot::Response;
  using ResponseOptions = marian::bergamot::ResponseOptions;
  using LanguageDirection = std::pair<std::string, std::string>;
  /// Initialize a service which wraps around service and allows
  /// translate(source-lang, target-lang, query)

  StaticTranslator(const Service::Config &config,
                   const std::vector<LanguageDirection> &directions,
                   const std::vector<std::string> &configFiles)
      : service_(config) {
    // Load all models into inventory.
    assert(directions.size() == configFiles.size());
    for (size_t i = 0; i < directions.size(); i++) {
      const LanguageDirection &direction = directions[i];
      const std::string &sourceLang = direction.first;
      const std::string &targetLang = direction.second;

      const std::string configPath = configFiles[i];

      std::cout << fmt::format("Loading {}-{} from {}", sourceLang, targetLang,
                               configPath)
                << std::endl;

      auto options = marian::bergamot::parseOptionsFromFilePath(configPath);
      Model model = service_.createCompatibleModel(options);
      models_.emplace(direction, model);
    }
  }

  std::optional<Response>
  translate(const LanguageDirection &direction, std::string input,
            const ResponseOptions &options = ResponseOptions()) {
    auto maybeModel = models_.find(direction);
    if (maybeModel == models_.end()) {
      return std::nullopt;
    }

    std::promise<Response> p;
    std::future<Response> f = p.get_future();
    auto callback = [&p](Response &&response) {
      p.set_value(std::move(response));
    };

    service_.translate(maybeModel->second, std::move(input), callback, options);
    f.wait();

    return std::optional<Response>(f.get());
  }

private:
  struct HashPair {
    size_t operator()(const LanguageDirection &direction) const {
      auto hash_combine = [](size_t &seed, size_t next) {
        seed ^=
            std::hash<size_t>{}(next) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      };

      size_t seed = std::hash<std::string>{}(direction.first);
      hash_combine(seed, std::hash<std::string>{}(direction.second));
      return seed;
    };
  };

  Service service_;
  std::unordered_map<LanguageDirection, Model, HashPair> models_;
};
} // namespace go_bergamot