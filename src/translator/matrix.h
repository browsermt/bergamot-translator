#pragma once

#include <vector>

namespace marian::bergamot {

struct Matrix {
  Matrix(Matrix &&other);
  Matrix(const size_t rowsParam, const size_t widthParam);

  void addRow();

  const float &at(const size_t row, const size_t col) const;
  float &at(const size_t row, const size_t col);

  size_t rows;
  const size_t cols;
  std::vector<float> data;
};
}  // namespace marian::bergamot