
#include "quality_model_factory.h"

#include "byte_array_util.h"
#include "logistic_regressor.h"
#include "response_options.h"
#include "simple_quality_model.h"

namespace marian::bergamot {

std::shared_ptr<IQualityModel> QualityModelFactory::Make(const AlignedMemory& qualityFileMemory) {
  // If no quality file return simple model
  if (qualityFileMemory.size() == 0) {
    return std::make_shared<SimpleQualityModel>();
  }

  return std::make_shared<LogisticRegressor>(LogisticRegressor::fromAlignedMemory(qualityFileMemory));
}
}  // namespace marian::bergamot
