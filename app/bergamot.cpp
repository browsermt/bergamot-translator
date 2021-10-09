#include "translator/byte_array_util.h"
#include "translator/parser.h"
#include "translator/response.h"
#include "translator/response_options.h"
#include "translator/service.h"
#include "translator/utils.h"

int main(int argc, char *argv[]) {
  using namespace marian::bergamot;
  ConfigParser<AsyncService> configParser;
  configParser.parseArgs(argc, argv);
  auto &config = configParser.getConfig();

  AsyncService service(config.serviceConfig);

  // Construct a model.
  auto options = parseOptionsFromFilePath(config.modelConfigPaths.front());

  MemoryBundle memoryBundle;
  std::shared_ptr<TranslationModel> model = service.createCompatibleModel(options, std::move(memoryBundle));

  ResponseOptions responseOptions;
  std::string input = readFromStdin();
  Response response = TranslateForResponse<AsyncService>()(service, model, std::move(input), responseOptions);

  std::cout << response.target.text;
  return 0;
}
