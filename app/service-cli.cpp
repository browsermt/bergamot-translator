#include <cstdlib>
#include <future>
#include <iostream>
#include <sstream>

#include "common/definitions.h"
#include "common/utils.h"
#include "marian.h"
#include "translator/parser.h"
#include "translator/print_utils.h"
#include "translator/response.h"
#include "translator/service.h"

int main(int argc, char *argv[]) {
  auto cp = marian::bergamot::createConfigParser();
  auto options = cp.parseOptions(argc, argv, true);
  marian::bergamot::Service service(options);

  // Read a large input text blob from stdin
  std::ostringstream std_input;
  std_input << std::cin.rdbuf();
  std::string input = std_input.str();
  using marian::bergamot::Response;

  marian::Ptr<marian::Options> responseOptions = marian::New<marian::Options>();
  responseOptions->set<bool>("quality", true);
  responseOptions->set<bool>("alignment", true);
  responseOptions->set<float>("alignment-threshold", 0.2f);

  // Wait on future until Response is complete
  std::future<Response> responseFuture =
      service.translate(std::move(input), responseOptions);
  responseFuture.wait();
  Response response = responseFuture.get();

  marian::bergamot::ResponsePrinter responsePrinter(response);
  responsePrinter.print(std::cout);

  return 0;
}
