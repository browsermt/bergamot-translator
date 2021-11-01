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

  auto callback = [](Response &&response) { std::cout << response.target.text; };

  service.translate(model, std::move(input), callback, responseOptions);

  // We will wait on destruction for thread-to join, so callback will be fired.

  return 0;
}
