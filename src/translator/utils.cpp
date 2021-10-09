#include "utils.h"

#include <future>

#include "response_options.h"
#include "service.h"
#include "translation_model.h"

namespace marian::bergamot {

std::string readFromStdin() {
  // Read a large input text blob from stdin
  std::ostringstream inputStream;
  inputStream << std::cin.rdbuf();
  std::string input = inputStream.str();
  return input;
}

}  // namespace marian::bergamot
