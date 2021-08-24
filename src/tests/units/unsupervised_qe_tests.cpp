#include "catch.hpp"
#include "test_helper.h"
#include "translator/unsupervised_qe.h"

namespace marian::bergamot {
class UnsupervisedQETest {
 public:
  Response::WordsQualityEstimate computeSentenceScores(const std::vector<float> &logProbs, const AnnotatedText &target,
                                                       const size_t sentenceIdx) const {
    return UnsupervisedQE::computeSentenceScores(logProbs, target, sentenceIdx);
  }
};
}  // namespace marian::bergamot


using namespace marian::bergamot;

SCENARIO("Unsupervised Quality Estimator test", "[UnsupervisedQE]") {
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

    AND_GIVEN("Simple Quality Estimator") {
      UnsupervisedQETest simpleQETest;
      WHEN("It's call computeSentenceScores") {
        auto wordsQualityEstimate = simpleQETest.computeSentenceScores(logProbs, annotatedTarget, 0);

        THEN("It's returned WordsQualityEstimate") {
          CHECK(wordsQualityEstimate.wordByteRanges ==
                std::vector<ByteRange>({{0, 1}, {2, 6}, {7, 9}, {10, 12}, {13, 21}}));

          CHECK(wordsQualityEstimate.wordQualityScores == std::vector<float>{-0.3, -0.0001, -0.002, -0.5, -0.15});
          CHECK(wordsQualityEstimate.sentenceScore == Approx(-0.19042f).epsilon(0.0001));
        }
      }
    }
  }
}
