#include "matrix.h"

namespace marian::bergamot {

Matrix::Matrix(const size_t rowsParam, const size_t colsParam) : rows(rowsParam), cols(colsParam), data(rows * cols) {}

Matrix::Matrix(Matrix&& other) : rows(other.rows), cols(other.cols), data(std::move(other.data)) {}

void Matrix::addRow() { data.resize((++rows) * cols); }

const float& Matrix::at(const size_t row, const size_t col) const { return data[row * cols + col]; }

float& Matrix::at(const size_t row, const size_t col) { return data[row * cols + col]; }

}  // namespace marian::bergamot