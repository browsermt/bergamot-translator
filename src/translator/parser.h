#ifndef SRC_BERGAMOT_PARSER_H
#define SRC_BERGAMOT_PARSER_H

#include "marian.h"

namespace marian {
namespace bergamot {

inline marian::ConfigParser createConfigParser() {
  marian::ConfigParser cp(marian::cli::mode::translation);
  cp.addOption<std::string>(
      "--ssplit-prefix-file", "Bergamot Options",
      "File with nonbreaking prefixes for sentence splitting.");

  cp.addOption<std::string>("--ssplit-mode", "Server Options",
                            "[paragraph, sentence, wrapped_text]", "paragraph");

  cp.addOption<int>(
      "--max-input-sentence-tokens", "Bergamot Options",
      "Maximum input tokens to be processed in a single sentence.", 128);

  cp.addOption<int>("--max-input-tokens", "Bergamot Options",
                    "Maximum input tokens in a batch. control for"
                    "Bergamot Queue",
                    1024);

  return cp;
}

} //  namespace bergamot
} //  namespace marian

#endif //  SRC_BERGAMOT_PARSER_H
