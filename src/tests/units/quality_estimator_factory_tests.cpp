#include "catch.hpp"
#include "translator/logistic_regressor.h"
#include "translator/quality_estimator_factory.h"
#include "translator/simple_quality_estimator.h"

using namespace marian::bergamot;

SCENARIO("Quality Estimator Factory test", "[QualityEstimatorFactory]") {
  WHEN("It's call Make with empty AlignedMemory") {
    AlignedMemory emptyMemory;
    const auto model = QualityEstimatorFactory::Make(emptyMemory);

    THEN("It's created a SimpleQualtiyModel") {
      CHECK(dynamic_cast<const SimpleQualityEstimator*>(model.get()) != nullptr);
    }
  }
  WHEN("It's call Make with a LR AlignedMemory") {
    std::vector<float> coefficients = {0.99000001, 0.899999976, -0.200000003, 0.5};
    const float intercept = {-0.300000012};

    LogisticRegressor::Scale scale;
    scale.stds = {0.200000003, 0.300000012, 2.5, 0.100000001};
    scale.means = {-0.100000001, -0.769999981, 5, -0.5};

    LogisticRegressor logisticRegressor(std::move(scale), std::move(coefficients), intercept);

    const auto model = QualityEstimatorFactory::Make(logisticRegressor.toAlignedMemory());

    THEN("It's created a LogistcRegressor") { CHECK(dynamic_cast<const LogisticRegressor*>(model.get()) != nullptr); }
  }
}
