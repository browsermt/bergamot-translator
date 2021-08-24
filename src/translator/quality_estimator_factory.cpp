
#include "quality_estimator_factory.h"

#include "byte_array_util.h"
#include "logistic_regressor_qe.h"
#include "response_options.h"
#include "unsupervised_qe.h"

namespace marian::bergamot {

std::shared_ptr<IQualityEstimator> QualityEstimatorFactory::Make(const AlignedMemory& qualityFileMemory) {
  // If no quality file return simple model
  if (qualityFileMemory.size() == 0) {
    return std::make_shared<UnsupervisedQE>();
  }

  return std::make_shared<LogisticRegressorQE>(LogisticRegressorQE::fromAlignedMemory(qualityFileMemory));
}
}  // namespace marian::bergamot
