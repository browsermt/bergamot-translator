#pragma once

#include <vector>

#include "matrix.h"

namespace marian::bergamot {

class IQualityModel {
 public:
  IQualityModel() = default;
  virtual ~IQualityModel() = default;

  virtual std::vector<float> predict(const Matrix &features) const = 0;
};
}  // namespace marian::bergamot
