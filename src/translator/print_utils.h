#pragma once

#include "response.h"
namespace marian {
namespace bergamot {

class ResponsePrinter {
public:
  ResponsePrinter(Response &response) : response_(&response) {}

  /// Prints a response into the provided ostream
  void print(std::ostream &out);

private:
  void sentence(std::ostream &out, size_t sentenceIdx);
  void alignments(std::ostream &out, size_t sentenceIdx);
  void quality(std::ostream &out, size_t sentenceIdx);

  /// Holds a reference to the response to be printed.
  Response *response_;
};
} // namespace bergamot
} // namespace marian
