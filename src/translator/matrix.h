#pragma once

#include <vector>

namespace marian::bergamot {
class Matrix {
 public:
  Matrix(Matrix &&other);
  Matrix(const size_t rows, const size_t cols);

  void addRow();

  const float &at(const size_t row, const size_t col) const;
  float &at(const size_t row, const size_t col);

  size_t rows() const;
  size_t cols() const;

 private:
  size_t rows_;
  const size_t cols_;
  std::vector<float> data_;
};
}  // namespace marian::bergamot