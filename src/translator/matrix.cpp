#include "matrix.h"

namespace marian::bergamot {

Matrix::Matrix(const size_t rows, const size_t cols) : rows_(rows), cols_(cols), data_(rows * cols) {}

Matrix::Matrix(Matrix&& other) : rows_(other.rows_), cols_(other.cols_), data_(std::move(other.data_)) {}

size_t Matrix::rows() const { return rows_; }

size_t Matrix::cols() const { return cols_; }

void Matrix::addRow() { data_.resize((++rows_) * cols_); }

const float& Matrix::at(const size_t row, const size_t col) const { return data_[row * cols_ + col]; }

float& Matrix::at(const size_t row, const size_t col) { return data_[row * cols_ + col]; }

}  // namespace marian::bergamot