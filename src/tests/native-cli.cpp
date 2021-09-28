#include "apps.h"
#include "common/logging.h"

using namespace marian::bergamot;

int main(int argc, char *argv[]) {
  createLoggers();
  ConfigParser configParser;
  configParser.parseArgs(argc, argv);
  auto &config = configParser.getConfig();
  AsyncService::Config serviceConfig;

  // TODO(improve)
  serviceConfig.numWorkers = config.numWorkers;
  serviceConfig.cacheEnabled = config.cacheEnabled;
  serviceConfig.cacheConfig = config.cacheConfig;

  AsyncService service(serviceConfig);

  TestSuite<AsyncService> testSuite(service);
  std::vector<std::shared_ptr<TranslationModel>> models;

  for (auto &modelConfigPath : config.modelConfigPaths) {
    TranslationModel::Config modelConfig = parseOptionsFromFilePath(modelConfigPath);
    std::shared_ptr<TranslationModel> model = service.createCompatibleModel(modelConfig);
    models.push_back(model);
  }

  switch (config.opMode) {
    case OpMode::TEST_SOURCE_SENTENCES:
      testSuite.annotatedTextSentences(models.front(), /*source=*/true);
      break;
    case OpMode::TEST_TARGET_SENTENCES:
      testSuite.annotatedTextSentences(models.front(), /*source=*/false);
      break;
    case OpMode::TEST_SOURCE_WORDS:
      testSuite.annotatedTextWords(models.front(), /*source=*/true);
      break;
    case OpMode::TEST_TARGET_WORDS:
      testSuite.annotatedTextWords(models.front(), /*source=*/false);
      break;
    case OpMode::TEST_FORWARD_BACKWARD_FOR_OUTBOUND:
      testSuite.forwardAndBackward(models);
      break;
    case OpMode::TEST_QUALITY_ESTIMATOR_WORDS:
      testSuite.qualityEstimatorWords(models.front());
      break;
    case OpMode::TEST_QUALITY_ESTIMATOR_SCORES:
      testSuite.qualityEstimatorScores(models.front());
      break;
    case OpMode::TEST_TRANSLATION_CACHE:
      testSuite.translationCache(models.front());
      break;
    case OpMode::TEST_CACHE_STORAGE_GROWTH:
      testSuite.wngt20IncrementalDecodingForCache(models.front());
      break;
    case OpMode::TEST_BENCHMARK_EDIT_WORKFLOW:
      testSuite.benchmarkCacheEditWorkflow(models.front());
      break;

    default:
      ABORT("Incompatible op-mode. Choose one of the test modes.");
      break;
  }
  return 0;
}
