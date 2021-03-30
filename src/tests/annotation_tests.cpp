#include "catch.hpp"
#include "translator/sentence_ranges.h"
#include <random>
#include <vector>

using namespace marian::bergamot;

TEST_CASE("Test Annotation API with random sentences") {
  /// Objective here is to test insertion for sentences, and that whatever comes
  /// out adheres to the way it was inserted. Towards this, we keep externally
  /// which sentence went in where and try to use accessor methods on
  /// AnnotatedText to check if what we have as ground-truth by construction is
  /// consistent with what is returned.
  size_t sentences = 20;
  size_t maxWords = 40;

  std::mt19937 randomIntGen_;
  randomIntGen_.seed(42);

  AnnotatedText testAnnotation;
  std::vector<std::vector<ByteRange>> sentenceWords;
  std::vector<ByteRange> Words;

  for (size_t idx = 0; idx < sentences; idx++) {
    if (idx != 0)
      testAnnotation.text += "\n";

    Words.clear();
    size_t words = randomIntGen_() % maxWords + 1;
    Words.reserve(words);
    for (size_t idw = 0; idw < words; idw++) {
      size_t before = testAnnotation.text.size();
      std::string word = std::to_string(idx) + "-" + std::to_string(idw);
      testAnnotation.text += word;
      if (idw != 0)
        testAnnotation.text += " ";
      Words.push_back((ByteRange){before, before + word.size() - 1});
    }
    // std::cout << std::endl;

    sentenceWords.push_back(Words);
  }

  // std::cout << "Inserting words:" << std::endl;
  std::vector<std::vector<marian::string_view>> byteRanges;
  for (auto &sentence : sentenceWords) {
    std::vector<marian::string_view> wordByteRanges;
    for (auto &word : sentence) {
      marian::string_view wordView(&testAnnotation.text[word.begin],
                                   word.end - word.begin);
      wordByteRanges.push_back(wordView);
      // std::cout << std::string(wordView) << " ";
    }
    testAnnotation.addSentence(wordByteRanges);
    byteRanges.push_back(wordByteRanges);
    // std::cout << std::endl;
  }

  // std::cout << "From container: " << std::endl;
  for (int idx = 0; idx < sentenceWords.size(); idx++) {
    for (int idw = 0; idw < sentenceWords[idx].size(); idw++) {
      ByteRange expected = sentenceWords[idx][idw];
      ByteRange obtained = testAnnotation.wordAsByteRange(idx, idw);
      // std::cout << std::string(testAnnotation.word(idx, idw)) << " ";
      CHECK(expected.begin == obtained.begin);
      CHECK(expected.end == obtained.end);

      std::string expected_string = std::string(byteRanges[idx][idw]);
      CHECK(expected_string == std::string(testAnnotation.word(idx, idw)));
    }
    // std::cout << std::endl;
  }
}
