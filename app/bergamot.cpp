#include "translator/byte_array_util.h"
#include "translator/parser.h"
#include "translator/response.h"
#include "translator/response_options.h"
#include "translator/service.h"
#include "translator/utils.h"

int main(int argc, char *argv[]) {
  using namespace marian::bergamot;
  ConfigParser<AsyncService> configParser("Bergamot CLI", /*multiOpMode=*/false);
  configParser.parseArgs(argc, argv);
  auto &config = configParser.getConfig();

  AsyncService service(config.serviceConfig);

  // Construct a model.
  auto options = parseOptionsFromFilePath(config.modelConfigPaths.front());

  std::shared_ptr<TranslationModel> model = service.createCompatibleModel(options);

  ResponseOptions responseOptions;
  std::string input = readFromStdin();

  // Create a barrier using future/promise.
  std::promise<Response> promise;
  std::future<Response> future = promise.get_future();
  auto callback = [&promise](Response &&response) {
    // Fulfill promise.
    promise.set_value(std::move(response));
  };

  service.translate(model, std::move(input), callback, responseOptions);

  // Wait until promise sets the response.
  Response response = future.get();

  // Print (only) translated text.
  std::cout << response.target.text;

  return 0;
}
