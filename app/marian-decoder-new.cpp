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
#include "translator/service.h"
#include "translator/translation_result.h"

void marian_decoder_minimal(const marian::Histories &histories,
                            marian::Ptr<marian::Vocab const> targetVocab,
                            marian::Ptr<marian::Options> options) {

  bool doNbest = options->get<bool>("n-best");
  auto collector =
      marian::New<marian::OutputCollector>(options->get<std::string>("output"));

  // There is a dependency of vocabs here.
  auto printer = marian::New<marian::OutputPrinter>(options, targetVocab);
  if (options->get<bool>("quiet-translation"))
    collector->setPrintingStrategy(marian::New<marian::QuietPrinting>());

  for (auto &history : histories) {
    std::stringstream best1;
    std::stringstream bestn;
    printer->print(history, best1, bestn);
    collector->Write((long)history->getLineNum(), best1.str(), bestn.str(),
                     doNbest);
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
  using marian::bergamot::TranslationResult;

  // Wait on future until TranslationResult is complete
  std::future<TranslationResult> translation_result_future =
      service.translate(std::move(input));
  translation_result_future.wait();
  const TranslationResult &translation_result = translation_result_future.get();

  marian_decoder_minimal(translation_result.histories(), service.targetVocab(),
                         options);

  LOG(info, "Total time: {:.5f}s wall", decoderTimer.elapsed());
  service.stop();
  return 0;
}
