#pragma once

#include <vector>

#include "iquality_model.h"

namespace marian::bergamot {

/// "Unsupervised" approach quality model
/// It's only return the mean of BPE tokens of a given word already compute by marian
class SimpleQualityModel : public IQualityModel {
 public:
  SimpleQualityModel() = default;
  ~SimpleQualityModel() = default;

  std::vector<float> predict(const Matrix &features) const override;
};

}  // namespace marian::bergamot