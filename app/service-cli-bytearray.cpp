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
#include "translator/byteArrayExample.h"

int main(int argc, char *argv[]) {
  auto cp = marian::bergamot::createConfigParser();
  auto options = cp.parseOptions(argc, argv, true);

  void * model_bytes = bergamot::getBinaryModelFromConfig(options);
  size_t model_memory_size = 0; // place-holder for size of model bytes
  std::vector<char> shortlist_bytes = bergamot::getBinaryShortlistFromConfig(options);
  marian::bergamot::Service service(options, model_bytes, model_memory_size,
                                    shortlist_bytes.data(), shortlist_bytes.size());

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
  free(model_bytes); // Ideally, this should be done after the translation model has been gracefully shut down.

  return 0;
}
