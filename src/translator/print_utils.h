#pragma once

#include "response.h"
namespace marian {
namespace bergamot {

class Printer {
public:
  Printer(Response &response) : response_(&response) {}
  void print(std::ostream &out) {
    text(out);
    sentences(out);
  }

private:
  void text(std::ostream &out);
  void sentences(std::ostream &out);
  void sentence(std::ostream &out, size_t sentenceIdx);
  void alignments(std::ostream &out, size_t sentenceIdx);
  void quality(std::ostream &out, size_t sentenceIdx);
  Response *response_;
};
} // namespace bergamot
} // namespace marian
