#include <cstdlib>
#include <future>
#include <iostream>
#include <sstream>

#include "common/definitions.h"
#include "common/utils.h"
#include "marian.h"
#include "translator/parser.h"
#include "translator/response.h"
#include "translator/service.h"
#include "translator/byte_array_util.h"

int main(int argc, char *argv[]) {
  auto cp = marian::bergamot::createConfigParser();
  auto options = cp.parseOptions(argc, argv, true);

  marian::bergamot::MemoryGift shortlistBytes = marian::bergamot::getBinaryShortlistFromConfig(options);
  marian::bergamot::MemoryGift modelBytes = marian::bergamot::getBinaryModelFromConfig(options);
  marian::bergamot::Service service(options, &modelBytes, &shortlistBytes);

  // Read a large input text blob from stdin
  std::ostringstream std_input;
  std_input << std::cin.rdbuf();
  std::string input = std_input.str();
  using marian::bergamot::Response;

  // Wait on future until Response is complete
  std::future<Response> responseFuture = service.translate(std::move(input));
  responseFuture.wait();
  Response response = responseFuture.get();
  std::cout << response.translation() << std::endl;

  // Clear the memory used for the byte array
  modelBytes.early_free(); // Ideally, this should be done after the translation model has been gracefully shut down.
  shortlistBytes.early_free();

  return 0;
}
