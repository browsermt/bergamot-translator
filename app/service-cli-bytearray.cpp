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

using namespace marian::bergamot;

int main(int argc, char *argv[]) {
  auto cp = marian::bergamot::createConfigParser();
  auto options = cp.parseOptions(argc, argv, true);

  // Prepare memories for model and shortlist
  std::string modelFile = getModelFileFromConfig(options);
  std::string shortlistFile = getShortlistFileFromConfig(options);
  size_t modelMemorySize = getMemorySizeFromFile(modelFile, true);
  size_t shortlistMemorySize = getMemorySizeFromFile(shortlistFile, false);

  AlignedMemory modelBytes(modelMemorySize);
  AlignedMemory shortlistBytes(shortlistMemorySize);
  loadFileToMemory(modelFile, modelBytes);
  loadFileToMemory(shortlistFile, shortlistBytes);

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
  std::cout << response.target.text << std::endl;

  return 0;
}
