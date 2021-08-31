#include "quality_estimator_tests.h"

#include "catch.hpp"
#include "translator/quality_estimator.h"

using namespace marian::bergamot;

SCENARIO("Logistic Regressor test", "[QualityEstimator]") {
  GIVEN("A feature matrix") {
    const std::vector<std::vector<float> > features = {{-0.3, -0.3, 1.0, -0.183683336},
                                                       {-0.0001, -0.0001, 1.0, -0.183683336},
                                                       {-0.002, -0.002, 1.0, -0.183683336},
                                                       {-0.5, -0.5, 1.0, -0.183683336},
                                                       {-0.15, -0.2, 2.0, -0.183683336}};

    LogisticRegressorQualityEstimator::Matrix featureMatrix(features.size(), features.begin()->size());

    for (int i = 0; i < features.size(); ++i) {
      for (int j = 0; j < features.begin()->size(); ++j) {
        featureMatrix.at(i, j) = features[i][j];
      }
    }

    AND_GIVEN("A LogistRegressor") {
      LogisticRegressorQualityEstimator::Array coefficients = {0.99000001, 0.899999976, -0.200000003, 0.5};
      const float intercept = {-0.300000012};

      LogisticRegressorQualityEstimator::Scale scale;
      scale.stds = {0.200000003, 0.300000012, 2.5, 0.100000001};
      scale.means = {-0.100000001, -0.769999981, 5, -0.5};

      LogisticRegressorQualityEstimator lrQE(std::move(scale), std::move(coefficients), intercept);

      WHEN("It's call predict") {
        const std::vector<float> prediction = lrQE.predict(featureMatrix);

        THEN("return the prediction") {
          CHECK(prediction == std::vector<float>{-2.14596, -4.41793, -4.403, -0.93204, -3.03343});
        }
      }

      WHEN("LR is construct by aligned memory") {
        const auto lrQEAlignedMemory = LogisticRegressorQualityEstimator::fromAlignedMemory(lrQE.toAlignedMemory());

        WHEN("It's call predict") {
          const std::vector<float> prediction = lrQEAlignedMemory.predict(featureMatrix);

          THEN("return the prediction") {
            CHECK(prediction == std::vector<float>{-2.14596, -4.41793, -4.403, -0.93204, -3.03343});
          }
        }
      }
    }
  }
}

bool operator==(const std::vector<float>& value1, const std::vector<float>& value2) {
  return std::equal(value1.begin(), value1.end(), value2.begin(), value2.end(), [](const auto& a, const auto& b) {
    auto value = Approx(b).epsilon(0.001);
    return a == value;
  });
}
