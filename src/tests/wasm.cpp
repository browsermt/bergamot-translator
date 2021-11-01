#include "common.h"
using namespace marian::bergamot;

void wasm(BlockingService &service, std::shared_ptr<TranslationModel> &model) {
  ResponseOptions responseOptions;
  std::vector<std::string> texts;

  // Hide the translateMultiple operation
  for (std::string line; std::getline(std::cin, line);) {
    texts.emplace_back(line);
  }

  auto results = service.translateMultiple(model, std::move(texts), responseOptions);

  for (auto &result : results) {
    std::cout << result.getTranslatedText() << std::endl;
  }
}

int main(int argc, char *argv[]) {
  ConfigParser<BlockingService> configParser;
  configParser.parseArgs(argc, argv);

  auto &config = configParser.getConfig();
  BlockingService service(config.serviceConfig);

  TestSuite<BlockingService> testSuite(service);
  std::vector<std::shared_ptr<TranslationModel>> models;

  for (auto &modelConfigPath : config.modelConfigPaths) {
    TranslationModel::Config modelConfig = parseOptionsFromFilePath(modelConfigPath);
    // Anything WASM is expected to use the byte-array-loads. So we hard-code grabbing MemoryBundle from FS and use the
    // MemoryBundle capable constructor.
    MemoryBundle memoryBundle = getMemoryBundleFromConfig(modelConfig);
    std::shared_ptr<TranslationModel> model = std::make_shared<TranslationModel>(modelConfig, std::move(memoryBundle));
    models.push_back(model);
  }

  /// WASM is one special case where WASM path is being checked, involving translateMultiple and a multi-line feed.
  /// Hence we do not bind it at a single input-blob single Response constraint imposed by the TestSuite.
  if (config.opMode == "wasm") {
    wasm(service, models.front());
  } else {
    testSuite.run(config.opMode, models);
  }

  return 0;
}
