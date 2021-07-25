#include "quality_helper.h"

#include <iostream>

#include "quality_estimator.h"

namespace marian {
namespace bergamot {

AlignedMemory QualityHelper::writeQualityEstimatorMemory(
    const std::vector<std::vector<float> >& logisticRegressorParameters) {
  size_t parametersDims = 4;

  QualityEstimator::Header header = {BINARY_QE_MODEL_MAGIC, logisticRegressorParameters.size() * sizeof(float)};

  marian::bergamot::AlignedMemory memory(sizeof(header) +
                                         logisticRegressorParameters.size() * parametersDims * sizeof(float));

  size_t index = 0;
  memcpy(memory.begin(), &header, sizeof(header));
  index += sizeof(header);

  for (const auto& parameters : logisticRegressorParameters) {
    for (const auto& value : parameters) {
      memcpy(memory.begin() + index, &value, sizeof(float));
      index += sizeof(float);
    }
  }

  return memory;
}
}  // namespace bergamot
}  // namespace marian
