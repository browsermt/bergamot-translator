
#include "apps.h"

int main(int argc, char *argv[]) {
  auto cp = marian::bergamot::createConfigParser();
  auto options = cp.parseOptions(argc, argv, true);
  const std::string mode = options->get<std::string>("bergamot-mode");
  using namespace marian::bergamot;
  if (mode == "test-quality-scores") {
    testapp::quality_scores(options);
  } else if (mode == "test-alignment-scores") {
    testapp::alignment_aggregated_to_source(options, /*numeric=*/true);
  } else if (mode == "test-alignment-words") {
    testapp::alignment_aggregated_to_source(options, /*numeric=*/false);
  } else if (mode == "test-response-source-sentences") {
    testapp::annotated_text_sentences(options, /*source=*/true);
  } else if (mode == "test-response-target-sentences") {
    testapp::annotated_text_sentences(options, /*source=*/false);
  } else if (mode == "test-response-source-words") {
    testapp::annotated_text_words(options, /*source=*/true);
  } else if (mode == "test-response-target-words") {
    testapp::annotated_text_words(options, /*source=*/false);
  } else if (mode == "legacy-service-cli") {
    testapp::legacy_service_cli(options);
  } else {
    ABORT("Unknown --mode {}. Please run a valid test", mode);
  }
  return 0;
}
