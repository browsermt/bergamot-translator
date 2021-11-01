#pragma once

#include <iostream>

namespace marian::bergamot {

inline std::string readFromStdin() {
  // Read a large input text blob from stdin
  std::ostringstream inputStream;
  inputStream << std::cin.rdbuf();
  std::string input = inputStream.str();
  return input;
}

}  // namespace marian::bergamot
