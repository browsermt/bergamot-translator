#pragma once

#include <vector>

#include "iquality_model.h"

namespace marian::bergamot {
class SimpleQualityModel : public IQualityModel {
 public:
  SimpleQualityModel() = default;
  ~SimpleQualityModel() = default;

  std::vector<float> predict(const Matrix &features) const override;
};

}  // namespace marian::bergamot