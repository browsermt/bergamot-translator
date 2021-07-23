#include "quality_estimator_tests.h"

#include <algorithm>

#include "catch.hpp"
#include "translator/quality_estimator.h"

using namespace marian::bergamot;

SCENARIO("Quality Estimator Test ", "[QualityEstimator]") {
  GIVEN("A quality, features and target") {
    // AnnotatedText Target
    std::string input = "This is an example.";
    marian::string_view preffix(input.data(), 0);

    std::string target = "- Este es un ejemplo.";

    std::vector<marian::string_view> sentencesView = {
        marian::string_view(target.data(), 1),       // "-"
        marian::string_view(target.data() + 1, 5),   // " Este"
        marian::string_view(target.data() + 6, 3),   // "  es"
        marian::string_view(target.data() + 9, 3),   // " un "
        marian::string_view(target.data() + 12, 8),  // " ejemplo",
        marian::string_view(target.data() + 20, 1),  // ".",
        marian::string_view(target.data() + 21, 0),  // "",
    };

    marian::bergamot::AnnotatedText annotatedTarget(std::move(std::string()));
    annotatedTarget.appendSentence(preffix, sentencesView.begin(), sentencesView.end());

    // LogProbs

    const std::vector<float> logProbs = {-0.3, -0.0001, -0.002, -0.5, -0.2, -0.1, -0.001};

    // Memory / Features

    size_t featureDims = 4;

    std::vector<float> stds = {0.200000003, 0.300000012, 2.5, 0.100000001};
    std::vector<float> means = {-0.100000001, -0.769999981, 5, -0.5};
    std::vector<float> coefficients = {0.99000001, 0.899999976, -0.200000003, 0.5};
    std::vector<float> intercept = {-0.300000012, -0.300000012, -0.300000012, -0.300000012};

    std::vector<std::vector<float> > features = {stds, means, coefficients, intercept};

    QualityEstimator::Header header = {BINARY_QE_MODEL_MAGIC, features.size() * sizeof(float)};

    marian::bergamot::AlignedMemory memory(sizeof(header) + features.size() * featureDims * sizeof(float));

    size_t index = 0;
    memcpy(memory.begin(), &header, sizeof(header));
    index += sizeof(header);

    for (const auto& feature : features) {
      for (const auto& value : feature) {
        memcpy(memory.begin() + index, &value, sizeof(float));
        index += sizeof(float);
      }
    }

    AND_GIVEN("QualityEstimator") {
      QualityEstimator qualityEstimator(std::move(memory));

      WHEN("It's call computeQualityScores") {
        auto wordsQualityEstimate = qualityEstimator.computeQualityScores(logProbs, annotatedTarget, 0);

        THEN("It's retuned WordsQualityEstimate") {
          CHECK(wordsQualityEstimate.wordByteRanges ==
                std::vector<ByteRange>({{0, 0}, {2, 5}, {7, 8}, {10, 11}, {13, 21}, {21, 21}}));

          CHECK(wordsQualityEstimate.wordQualityScores ==
                std::vector<float>({0.883, 0.988, 0.988, 0.606, 0.952, 0.988}));
          CHECK(wordsQualityEstimate.sentenceScore == Approx(0.90075));
        }
      }
    }
  }
}

bool operator==(const std::vector<float>& value1, const std::vector<float>& value2) {
  return std::equal(value1.begin(), value1.end(), value2.begin(), value2.end(), [](const auto& a, const auto& b) {
    auto value = Approx(b).epsilon(0.005);
    // value.setEpsilon(0.005);
    return a == value;
  });
}

bool operator==(const std::vector<ByteRange>& value1, const std::vector<ByteRange>& value2) {
  return std::equal(value1.begin(), value1.end(), value2.begin(), value2.end(),
                    [](const auto& a, const auto& b) { return (a.begin == b.begin) && (a.end == b.end); });
}

std::ostream& operator<<(std::ostream& os, const marian::bergamot::ByteRange& value) {
  os << "{begin: " << value.begin << ", end: " << value.end << "} ";
  return os;
}