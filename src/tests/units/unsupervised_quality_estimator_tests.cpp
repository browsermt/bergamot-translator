#include "catch.hpp"
#include "test_helper.h"
#include "translator/unsupervised_quality_estimator.h"

using namespace marian::bergamot;

SCENARIO("Unsupervised Quality Estimator test", "[UnsupervisedQualityEstimator]") {
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

    AND_GIVEN("Unsupervised Quality Estimator") {
      WHEN("It's call computeQualityScores") {
        UnsupervisedQualityEstimator unsupervisedQE;
        unsupervisedQE.computeQualityScores(histories, response);

        THEN("It's returned WordsQualityEstimate") {
          REQUIRE(response.qualityScores.size() == 1);

          const auto& wordsQualityEstimate = response.qualityScores[0];

          CHECK(wordsQualityEstimate.wordByteRanges ==
                std::vector<ByteRange>({{0, 1}, {2, 6}, {7, 9}, {10, 12}, {13, 21}}));

          CHECK(wordsQualityEstimate.wordQualityScores == std::vector<float>{-0.3, -0.0001, -0.002, -0.5, -0.15});
          CHECK(wordsQualityEstimate.sentenceScore == Approx(-0.19042f).epsilon(0.0001));
        }
      }
    }
  }
}
