#include "common.h"
#include "translator/parser.h"
#include "translator/service.h"
#include "translator/translation_model.h"

using namespace marian::bergamot;

int main(int argc, char *argv[]) {
  ConfigParser<AsyncService> configParser;
  configParser.parseArgs(argc, argv);
  auto &config = configParser.getConfig();

  AsyncService service(config.serviceConfig);

  std::vector<std::shared_ptr<TranslationModel>> models;

  for (auto &modelConfigPath : config.modelConfigPaths) {
    TranslationModel::Config modelConfig = parseOptionsFromFilePath(modelConfigPath);
    std::shared_ptr<TranslationModel> model = service.createCompatibleModel(modelConfig);
    models.push_back(model);
  }

  TestSuite<AsyncService> testSuite(service);
  testSuite.run(config.opMode, models);

  return 0;
}
