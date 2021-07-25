#pragma once

#include "definitions.h"

namespace marian {
namespace bergamot {

/// QualityEstimatorHelper is a utility function that writes a QualityEstimator Aligned Memory
class QualityHelper {
 public:
  static AlignedMemory writeQualityEstimatorMemory(const std::vector<std::vector<float> >& logisticRegressorParameters);
};
}  // namespace bergamot
}  // namespace marian