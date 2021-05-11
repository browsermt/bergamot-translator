#include <cstdlib>
#include <future>
#include <iostream>
#include <sstream>

#include "common/definitions.h"
#include "common/timer.h"
#include "common/utils.h"
#include "marian.h"
#include "translator/history.h"
#include "translator/output_collector.h"
#include "translator/output_printer.h"
#include "translator/parser.h"
#include "translator/response.h"
#include "translator/service.h"

void marian_decoder_minimal(const marian::bergamot::Response &response,
                            marian::Ptr<marian::Options> options) {
  // We are no longer marian-decoder compatible. Server ideas are on hold.
  for (size_t sentenceIdx = 0; sentenceIdx < response.size(); sentenceIdx++) {
    std::cout << response.target.sentence(sentenceIdx) << "\n";
  }
}

int main(int argc, char *argv[]) {
  auto cp = marian::bergamot::createConfigParser();
  auto options = cp.parseOptions(argc, argv, true);
  marian::timer::Timer decoderTimer;

  marian::bergamot::Service service(options);
  // Read a large input text blob from stdin
  std::ostringstream std_input;
  std_input << std::cin.rdbuf();
  std::string input = std_input.str();
  using marian::bergamot::Response;

  marian::bergamot::ResponseOptions responseOptions;
  std::promise<Response> responsePromise;
  std::future<Response> responseFuture = responsePromise.get_future();
  auto callback = [&responsePromise](Response &&response) {
    responsePromise.set_value(std::move(response));
  };

  service.translate(std::move(input), std::move(callback), responseOptions);
  responseFuture.wait();
  Response response = responseFuture.get();

  marian_decoder_minimal(response, options);

  LOG(info, "Total time: {:.5f}s wall", decoderTimer.elapsed());
  return 0;
}
