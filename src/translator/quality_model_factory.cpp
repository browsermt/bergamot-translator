
#include "quality_model_factory.h"

#include "logistic_regressor.h"
#include "response_options.h"
#include "byte_array_util.h"

namespace marian::bergamot {

std::shared_ptr<IQualityModel> QualityModelFactory::Make(const Ptr<Options>& options,
                                                         const MemoryBundle& memoryBundle) {
  const auto qualityType = options->get<int>("quality-type", QualityScoreType::SIMPLE);

  ABORT_IF((qualityType < BEGIN_VALID_TYPE && qualityType > END_VALID_TYPE), "Invalid quality-score type");

  switch (qualityType) {
    case QualityScoreType::SIMPLE: {
      //@TODO
      return nullptr;
    }

    case QualityScoreType::LR: {
      const auto qualityFile = options->get<std::string>("quality-file", "");

      ABORT_IF((qualityType == QualityScoreType::LR && qualityFile.empty()),
               "No quality file pass for LR quality estimator");

      if (memoryBundle.qualityEstimatorMemory.size() != 0) {
        return std::make_shared<LogisticRegressor>(
            LogisticRegressor::fromAlignedMemory(memoryBundle.qualityEstimatorMemory));
      }

      return std::make_shared<LogisticRegressor>(
          LogisticRegressor::fromAlignedMemory(getQualityEstimatorModel(options)));
    }

    default:
    {
       return nullptr;
    }
  }
}
}  // namespace marian::bergamot
