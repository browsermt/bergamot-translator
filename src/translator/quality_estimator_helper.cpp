
#include "quality_estimator_helper.h"

#include "byte_array_util.h"
#include "logistic_regressor_quality_estimator.h"
#include "response_options.h"
#include "unsupervised_quality_estimator.h"

namespace marian::bergamot {

std::shared_ptr<IQualityEstimator> createQualityEstimator(const AlignedMemory& qualityFileMemory) {
  // If no quality file return simple model
  if (qualityFileMemory.size() == 0) {
    return std::make_shared<UnsupervisedQualityEstimator>();
  }

  return std::make_shared<LogisticRegressorQualityEstimator>(
      LogisticRegressorQualityEstimator::fromAlignedMemory(qualityFileMemory));
}
}  // namespace marian::bergamot
