#include "common.h"
using namespace marian::bergamot;

void wasm(BlockingService &service, std::shared_ptr<TranslationModel> &model) {
  std::vector<std::string> plainTexts = {
      "Hello World!",                                 //
      "The quick brown fox jumps over the lazy dog."  //
  };

  std::vector<std::string> htmlTexts = {
      "<a href=\"#\">Hello</a> world.",                                                   //
      "The quick brown <b id=\"fox\">fox</b> jumps over the lazy <i id=\"dog\">dog</i>."  //
  };

  std::vector<std::string> texts;
  std::vector<ResponseOptions> options;
  for (auto &plainText : plainTexts) {
    texts.push_back(plainText);
    ResponseOptions opt;
    options.push_back(opt);
  }

  for (auto &htmlText : htmlTexts) {
    texts.push_back(htmlText);
    ResponseOptions opt;
    opt.HTML = true;
    options.push_back(opt);
  }

  auto results = service.translateMultiple(model, std::move(texts), options);

  for (auto &result : results) {
    std::cout << result.getTranslatedText() << std::endl;
  }
}

int main(int argc, char *argv[]) {
  ConfigParser<BlockingService> configParser("WebAssembly test-suite", /*multiOpMode=*/true);
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
