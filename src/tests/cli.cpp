#include "apps.h"

int main(int argc, char *argv[]) {
  using namespace marian::bergamot;
  marian::bergamot::ConfigParser configParser;
  configParser.parseArgs(argc, argv);
  auto &config = configParser.getConfig();
  AsyncService::Config serviceConfig{config.numWorkers};
  AsyncService service(serviceConfig);
  std::vector<std::shared_ptr<TranslationModel>> models;

  for (auto &modelConfigPath : config.modelConfigPaths) {
    TranslationModel::Config modelConfig = parseOptionsFromFilePath(modelConfigPath);
    std::shared_ptr<TranslationModel> model = service.createCompatibleModel(modelConfig);
    models.push_back(model);
  }

  switch (config.opMode) {
    case OpMode::TEST_SOURCE_SENTENCES:
      testapp::annotatedTextSentences(service, models.front(), /*source=*/true);
      break;
    case OpMode::TEST_TARGET_SENTENCES:
      testapp::annotatedTextSentences(service, models.front(), /*source=*/false);
      break;
    case OpMode::TEST_SOURCE_WORDS:
      testapp::annotatedTextWords(service, models.front(), /*source=*/true);
      break;
    case OpMode::TEST_TARGET_WORDS:
      testapp::annotatedTextWords(service, models.front(), /*source=*/false);
      break;
    case OpMode::TEST_FORWARD_BACKWARD_FOR_OUTBOUND:
      testapp::forwardAndBackward(service, models);
      break;
    case OpMode::TEST_QUALITY_ESTIMATOR_WORDS:
      testapp::qualityEstimatorWords(service, models.front());
      break;
    case OpMode::TEST_QUALITY_ESTIMATOR_SCORES:
      testapp::qualityEstimatorScores(service, models.front());
      break;
    case OpMode::TEST_MULTIMODELS_INTENSIVE:
      testapp::concurrentMultimodelsIntensive(service, models);
      break;
    default:
      ABORT("Incompatible op-mode. Choose one of the test modes.");
      break;
  }
  return 0;
}
