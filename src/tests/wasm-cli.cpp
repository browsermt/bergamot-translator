#include "apps.h"
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

  switch (config.opMode) {
    case OpMode::TEST_WASM_PATH:
      wasm(service, models.front());
      break;

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
