#include "simple_quality_model.h"

namespace marian::bergamot {
std::vector<float> SimpleQualityModel::predict(const Matrix &features) const {
  const size_t I_MEAN{0};
  std::vector<float> predictions(features.rows());

  for (size_t i = 0; i < features.rows(); ++i) {
    predictions[i] = features.at(i, I_MEAN);
  }

  return predictions;
}
}  // namespace marian::bergamot