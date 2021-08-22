#include "test_helper.h"

#include "catch.hpp"
#include "translator/logistic_regressor.h"

using namespace marian::bergamot;

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
      std::vector<float> coefficients = {0.99000001, 0.899999976, -0.200000003, 0.5};
      const float intercept = {-0.300000012};

      LogisticRegressor::Scale scale;
      scale.stds = {0.200000003, 0.300000012, 2.5, 0.100000001};
      scale.means = {-0.100000001, -0.769999981, 5, -0.5};

      LogisticRegressor logisticRegressor(std::move(scale), std::move(coefficients), intercept);

      WHEN("It's call predict") {
        const std::vector<float> prediction = logisticRegressor.predict(featureMatrix);

        THEN("return the prediction") { CHECK(prediction == std::vector<float>{0.883, 0.988, 0.988, 0.606, 0.952}); }
      }
    }
  }
}

SCENARIO("Logistic Regressor Test", "[QualityEstimator]") {
  GIVEN("A quality, features and target") {
    // AnnotatedText Target
    std::string input = "This is an example.";
    marian::string_view prefix(input.data(), 0);

    std::string target = "- Este es un ejemplo.";

    std::vector<marian::string_view> sentencesView = {
        marian::string_view(target.data(), 1),       // "-"
        marian::string_view(target.data() + 1, 5),   // " Este"
        marian::string_view(target.data() + 6, 3),   // "  es"
        marian::string_view(target.data() + 9, 3),   // " un"
        marian::string_view(target.data() + 12, 8),  // " ejemplo",
        marian::string_view(target.data() + 20, 1),  // ".",
        marian::string_view(target.data() + 21, 0),  // "",
    };

    marian::bergamot::AnnotatedText annotatedTarget(std::move(std::string()));
    annotatedTarget.appendSentence(prefix, sentencesView.begin(), sentencesView.end());

    // LogProbs

    const std::vector<float> logProbs = {-0.3, -0.0001, -0.002, -0.5, -0.2, -0.1, -0.001};

    // Memory / Features

    LogisticRegressor::Scale scale;
    scale.stds = {0.200000003, 0.300000012, 2.5, 0.100000001};
    scale.means = {-0.100000001, -0.769999981, 5, -0.5};

    std::vector<float> coefficients = {0.99000001, 0.899999976, -0.200000003, 0.5};
    const float intercept = {-0.300000012};

    AND_GIVEN("LogisticRegressor Quality Estimator") {
      LogisticRegressor logisticRegressor(std::move(scale), std::move(coefficients), intercept);

      WHEN("It's call computeQualityScores") {
        auto wordsQualityEstimate = logisticRegressor.computeQualityScores(logProbs, annotatedTarget, 0);

        THEN("It's returned WordsQualityEstimate") {
          CHECK(wordsQualityEstimate.wordByteRanges ==
                std::vector<ByteRange>({{0, 1}, {2, 6}, {7, 9}, {10, 12}, {13, 21}}));

          CHECK(wordsQualityEstimate.wordQualityScores == std::vector<float>({0.883, 0.988, 0.988, 0.606, 0.952}));
          CHECK(wordsQualityEstimate.sentenceScore == Approx(0.88341f).epsilon(0.0001));
        }
      }
    }
  }
}
