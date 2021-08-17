#include "catch.hpp"
#include "translator/logistic_regressor.h"
#include "translator/quality_model_factory.h"
#include "translator/simple_quality_model.h"

using namespace marian::bergamot;

SCENARIO("Quality Model Factory test", "[QualityModelFactory]") {
  WHEN("It's call Make with empty AlignedMemory") {
    AlignedMemory emptyMemory;
    const auto model = QualityModelFactory::Make(emptyMemory);

    THEN("It's created a SimpleQualtiyModel") {
      CHECK(dynamic_cast<const SimpleQualityModel*>(model.get()) != nullptr);
    }
  }
  WHEN("It's call Make with a LR AlignedMemory") {
    std::vector<float> coefficients = {0.99000001, 0.899999976, -0.200000003, 0.5};
    const float intercept = {-0.300000012};

    LogisticRegressor::Scale scale;
    scale.stds = {0.200000003, 0.300000012, 2.5, 0.100000001};
    scale.means = {-0.100000001, -0.769999981, 5, -0.5};

    LogisticRegressor logisticRegressor(std::move(scale), std::move(coefficients), intercept);

    const auto model = QualityModelFactory::Make(logisticRegressor.toAlignedMemory());

    THEN("It's created a LogistcRegressor") { CHECK(dynamic_cast<const LogisticRegressor*>(model.get()) != nullptr); }
  }
}
