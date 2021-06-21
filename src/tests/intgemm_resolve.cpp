#include <iostream>

#include "intgemm/intgemm.h"

int main() {
  std::cout << static_cast<int>(intgemm::kCPU) << "\n";
  return 0;
}
