#include "catch.hpp"
#include "translator/simple_quality_model.h"

using namespace marian::bergamot;

bool operator==(const std::vector<float>& value1, const std::vector<float>& value2) {
  return std::equal(value1.begin(), value1.end(), value2.begin(), value2.end(), [](const auto& a, const auto& b) {
    auto value = Approx(b).epsilon(0.005);
    return a == value;
  });
}

SCENARIO("Simple Quality Model test", "[SimpleQualityModel]") {
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

    AND_GIVEN("A SimpleQualityModel") {
      SimpleQualityModel model;

      WHEN("It's call predict") {
        const std::vector<float> prediction = model.predict(featureMatrix);

        THEN("return the prediction") { CHECK(prediction == std::vector<float>{-0.3, -0.0001, -0.002, -0.5, -0.15}); }
      }
    }
  }
}
