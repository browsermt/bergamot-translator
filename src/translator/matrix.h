#pragma once

#include "definitions.h"
#include "intgemm/intgemm.h"

namespace marian::bergamot {

struct Matrix {
  Matrix(Matrix &&other);
  Matrix(const size_t rowsParam, const size_t widthParam);

  const float &at(const size_t row, const size_t col) const;
  float &at(const size_t row, const size_t col);

  const size_t rows;
  const size_t cols;
  AlignedVector<float> data;
};

/// A strucut to simplify the usage of intgemm
struct IntgemmMatrix : Matrix {
  IntgemmMatrix(const intgemm::Index rowsParam, const intgemm::Index widthParam, const intgemm::Index rowsMultiplier,
                const intgemm::Index widthMultiplier);

  IntgemmMatrix(IntgemmMatrix &&other);

  Matrix operator*(const IntgemmMatrix &matrixb) const;
};
}  // namespace marian::bergamot