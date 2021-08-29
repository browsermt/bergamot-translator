#include "catch.hpp"
#include "translator/logistic_regressor_quality_estimator.h"
#include "translator/quality_estimator_helper.h"
#include "translator/unsupervised_quality_estimator.h"

using namespace marian::bergamot;

SCENARIO("Create Quality Estimator test", "[QualityEstimatorHelper]") {
  WHEN("It's call Make with a empty AlignedMemory") {
    AlignedMemory emptyMemory;
    const auto model = createQualityEstimator(emptyMemory);

    THEN("It's created a UnsupervisedQualityEstimator") {
      CHECK(dynamic_cast<const UnsupervisedQualityEstimator*>(model.get()) != nullptr);
    }
  }
  WHEN("It's call Make with a LR AlignedMemory") {
    std::vector<float> coefficients = {0.99000001, 0.899999976, -0.200000003, 0.5};
    const float intercept = {-0.300000012};

    LogisticRegressorQualityEstimator::Scale scale;
    scale.stds = {0.200000003, 0.300000012, 2.5, 0.100000001};
    scale.means = {-0.100000001, -0.769999981, 5, -0.5};

    LogisticRegressorQualityEstimator logisticRegressor(std::move(scale), std::move(coefficients), intercept);

    const auto model = createQualityEstimator(logisticRegressor.toAlignedMemory());

    THEN("It's created a LogisticRegressor") {
      CHECK(dynamic_cast<const LogisticRegressorQualityEstimator*>(model.get()) != nullptr);
    }
  }
}
