#include "catch.hpp"
#include "test_helper.h"
#include "translator/logistic_regressor_quality_estimator.h"

using namespace marian::bergamot;

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

    Response response;

    response.target.appendSentence(prefix, sentencesView.begin(), sentencesView.end());

    // Histories - LogProbs

    const std::vector<float> logProbs = {-0.3, -0.0001, -0.002, -0.5, -0.2, -0.1, -0.001};

    auto histories = logProbsToHistories(logProbs);

    // Memory / Features

    LogisticRegressorQualityEstimator::Scale scale;
    scale.stds = {0.200000003, 0.300000012, 2.5, 0.100000001};
    scale.means = {-0.100000001, -0.769999981, 5, -0.5};

    std::vector<float> coefficients = {0.99000001, 0.899999976, -0.200000003, 0.5};
    const float intercept = {-0.300000012};

    AND_GIVEN("LogisticRegressorQualityEstimator Quality Estimator") {
      LogisticRegressorQualityEstimator logisticRegressorQE(std::move(scale), std::move(coefficients), intercept);

      WHEN("It's call computeQualityScores") {
        logisticRegressorQE.computeQualityScores(histories, response);

        THEN("It's add WordsQualityEstimate on response") {
          REQUIRE(response.qualityScores.size() == 1);

          const auto& wordsQualityEstimate = response.qualityScores[0];

          CHECK(wordsQualityEstimate.wordByteRanges ==
                std::vector<ByteRange>({{0, 1}, {2, 6}, {7, 9}, {10, 12}, {13, 21}}));

          CHECK(wordsQualityEstimate.wordQualityScores ==
                std::vector<float>({-2.14596, -4.41793, -4.403, -0.93204, -3.03343}));
          CHECK(wordsQualityEstimate.sentenceScore == Approx(-2.98647).epsilon(0.0001));
        }
      }
    }
  }
}
