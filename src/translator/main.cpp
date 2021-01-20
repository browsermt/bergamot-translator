#include <cstdlib>
#include <iostream>
#include <sstream>

#include "common/definitions.h"
#include "common/timer.h"
#include "common/utils.h"
#include "marian.h"
#include "translator/history.h"
#include "translator/output_collector.h"
#include "translator/output_printer.h"

#include "service.h"

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
  marian::ConfigParser cp(marian::cli::mode::translation);

  cp.addOption<std::string>(
      "--ssplit-prefix-file", "Bergamot Options",
      "File with nonbreaking prefixes for sentence splitting.");

  cp.addOption<std::string>("--ssplit-mode", "Server Options",
                            "[paragraph, sentence, wrapped_text]");

  cp.addOption<int>(
      "--max-input-sentence-tokens", "Bergamot Options",
      "Maximum input tokens to be processed in a single sentence.", 128);

  cp.addOption<int>("--max-input-tokens", "Bergamot Options",
                    "Maximum input tokens in a batch. control for"
                    "Bergamot Queue",
                    1024);

  cp.addOption<int>("--nbest", "Bergamot Options",
                    "NBest value used for decoding", 1);

  cp.addOption<bool>("--marian-decoder-alpha", "Bergamot Options",
                     "Run marian-decoder output printer code", false);

  // TODO(jerin): Add QE later.
  // marian::qe::QualityEstimator::addOptions(cp);

  marian::timer::Timer decoderTimer;

  auto options = cp.parseOptions(argc, argv, true);
  marian::bergamot::Service service(options);

  std::ostringstream std_input;
  std_input << std::cin.rdbuf();
  std::string input = std_input.str();

  LOG(info, "IO complete Translating input");
  auto translation_result_future = service.translate(std::move(input));
  translation_result_future.wait();
  auto translation_result = translation_result_future.get();
  if (options->get<bool>("marian-decoder-alpha")) {
    marian_decoder_minimal(translation_result.getHistories(),
                           service.targetVocab(), options);
    LOG(info, "Total time: {:.5f}s wall", decoderTimer.elapsed());
  } else {
    for (auto &p : translation_result.getSentenceMappings()) {
      std::cout << "[src] " << p.first << "\n";
      std::cout << "[tgt] " << p.second << "\n";
    }
  }

  service.stop();
  return 0;
}
