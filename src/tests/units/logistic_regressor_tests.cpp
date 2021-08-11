#include "catch.hpp"
#include "translator/logistic_regressor.h"

using namespace marian::bergamot;

bool operator==(const std::vector<float>& value1, const std::vector<float>& value2) {
  return std::equal(value1.begin(), value1.end(), value2.begin(), value2.end(), [](const auto& a, const auto& b) {
    auto value = Approx(b).epsilon(0.005);
    return a == value;
  });
}

SCENARIO("Logistic Regressor test", "[LogisticRegressor]") {
  GIVEN("A feature matrix") {
    const std::vector<std::vector<float> > features = {{-0.3, -0.3, 1.0, -0.183683336},
                                                       {-0.0001, -0.0001, 1.0, -0.183683336},
                                                       {-0.002, -0.002, 1.0, -0.183683336},
                                                       {-0.5, -0.5, 1.0, -0.183683336},
                                                       {-0.15, -0.2, 2.0, -0.183683336}};

    Matrix featureMatrix(features.size(), features.begin()->size());

    for (int i = 0; i < features.size(); ++i) {
      for (int j = 0; j < features.begin()->size(); ++j) {
        featureMatrix.at(i, j) = features[i][j];
      }
    }

    AND_GIVEN("A LogistRegressor") {
      const std::vector<float> coefficients = {0.99000001, 0.899999976, -0.200000003, 0.5};
      const float intercept = {-0.300000012};

      LogisticRegressor::Scale scale;
      scale.stds = {0.200000003, 0.300000012, 2.5, 0.100000001};
      scale.means = {-0.100000001, -0.769999981, 5, -0.5};

      LogisticRegressor logisticRegressor(std::move(scale), coefficients, intercept);

      WHEN("It's call predict") {
        const std::vector<float> prediction = logisticRegressor.predict(featureMatrix);

        THEN("return the prediction") { CHECK(prediction == std::vector<float>{0.883, 0.988, 0.988, 0.606, 0.952}); }
      }
    }
  }
}
