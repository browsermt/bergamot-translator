#include "simple_quality_model.h"

namespace marian::bergamot {
std::vector<float> SimpleQualityModel::predict(const Matrix &features) const {
  const size_t I_MEAN{0};
  std::vector<float> predicitions;

  for (size_t i = 0; i < features.rows; ++i) {
    predicitions.push_back(features.at(i, I_MEAN));
  }

  return predicitions;
}
}  // namespace marian::bergamot