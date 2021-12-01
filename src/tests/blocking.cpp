#include "common.h"
using namespace marian::bergamot;

int main(int argc, char *argv[]) {
  ConfigParser<BlockingService> configParser;
  configParser.parseArgs(argc, argv);

  auto &config = configParser.getConfig();
  BlockingService service(config.serviceConfig);

  TestSuite<BlockingService> testSuite(service);
  std::vector<std::shared_ptr<TranslationModel>> models;

  for (auto &modelConfigPath : config.modelConfigPaths) {
    TranslationModel::Config modelConfig = parseOptionsFromFilePath(modelConfigPath);
    std::shared_ptr<TranslationModel> model = std::make_shared<TranslationModel>(modelConfig);
    models.push_back(model);
  }

  /// WASM is one special case where WASM path is being checked, involving translateMultiple and a multi-line feed.
  /// Hence we do not bind it at a single input-blob single Response constraint imposed by the TestSuite.
  testSuite.run(config.opMode, models);

  return 0;
}
