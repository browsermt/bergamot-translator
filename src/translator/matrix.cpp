#include "matrix.h"

namespace marian::bergamot {

constexpr intgemm::Index getNextMultiple(const intgemm::Index number, const intgemm::Index multiple) {
  const intgemm::Index modWidth = number % multiple;
  return (modWidth == 0) ? number : number + multiple - modWidth;
}

Matrix::Matrix(const size_t rowsParam, const size_t colsParam) : rows(rowsParam), cols(colsParam), data(rows * cols) {
  for (float& elem : data) {
    elem = 0.0;
  }
}

Matrix::Matrix(Matrix&& other) : rows(other.rows), cols(other.cols), data(std::move(other.data)) {}

const float& Matrix::at(const size_t row, const size_t col) const { return data[row * cols + col]; }

float& Matrix::at(const size_t row, const size_t col) { return data[row * cols + col]; }

IntgemmMatrix::IntgemmMatrix(const intgemm::Index rowsParam, const intgemm::Index colsParam,
                             const intgemm::Index rowsMultiplier, const intgemm::Index colsMultiplier)
    : Matrix(getNextMultiple(rowsParam, rowsMultiplier), getNextMultiple(colsParam, colsMultiplier)) {}

IntgemmMatrix::IntgemmMatrix(IntgemmMatrix&& other) : Matrix(std::move(other)) {}

Matrix IntgemmMatrix::operator*(const IntgemmMatrix& matrixb) const {
  const float quant_mult = 1024.0f;

  AlignedVector<int16_t> A_prepared(data.size());
  AlignedVector<int16_t> B_prepared(matrixb.data.size());

  intgemm::Int16::PrepareA(data.begin(), A_prepared.begin(), quant_mult, rows, cols);
  intgemm::Int16::PrepareB(matrixb.data.begin(), B_prepared.begin(), quant_mult, matrixb.rows, matrixb.cols);

  Matrix result(rows, matrixb.cols);

  intgemm::Int16::Multiply(
      A_prepared.begin(), B_prepared.begin(), rows, cols, matrixb.cols,
      intgemm::callbacks::UnquantizeAndWrite(1.0f / (quant_mult * quant_mult), result.data.begin()));

  return result;
}

}  // namespace marian::bergamot