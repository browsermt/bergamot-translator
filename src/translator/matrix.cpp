#include "matrix.h"

namespace marian::bergamot {

Matrix::Matrix(const size_t rowsParam, const size_t colsParam) : rows(rowsParam), cols(colsParam), data_(rowsParam * colsParam) {}

Matrix::Matrix(Matrix&& other) : rows(other.rows), cols(other.cols), data_(std::move(other.data_)) {}

const float& Matrix::at(const size_t row, const size_t col) const { return data_[row * cols + col]; }

float& Matrix::at(const size_t row, const size_t col) { return data_[row * cols + col]; }

}  // namespace marian::bergamot
