#pragma once

#include <vector>

namespace marian::bergamot {
class Matrix {
 public:
  const size_t rows;
  const size_t cols;

  Matrix(Matrix &&other);
  Matrix(const size_t rowsParam, const size_t colsParam);

  const float &at(const size_t row, const size_t col) const;
  float &at(const size_t row, const size_t col);

 private:
  std::vector<float> data_;
};
}  // namespace marian::bergamot
