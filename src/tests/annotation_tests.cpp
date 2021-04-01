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

  bool debug{false}; // Set in case needed to see output.

  std::mt19937 randomIntGen_;
  randomIntGen_.seed(42);

  AnnotatedText testAnnotation;
  std::vector<std::vector<ByteRange>> sentenceWords;
  std::vector<ByteRange> wordByteRanges;
  std::vector<ByteRange> sentenceByteRanges;

  // Prepare the text and construct ByteRanges as intended for sentences and
  // words. The ByteRanges we construct here are expected to be the
  // ground-truths for words and sentences.
  for (size_t idx = 0; idx < sentences; idx++) {
    if (idx != 0)
      testAnnotation.text += "\n";

    wordByteRanges.clear();
    size_t words = randomIntGen_() % maxWords + 1;
    wordByteRanges.reserve(words);
    size_t sentenceBegin, sentenceEnd;
    for (size_t idw = 0; idw < words; idw++) {
      if (idw != 0)
        testAnnotation.text += " ";
      size_t before = testAnnotation.text.size();
      std::string word = std::to_string(idx) + "-" + std::to_string(idw);
      testAnnotation.text += word;
      wordByteRanges.push_back((ByteRange){before, before + word.size()});

      if (idw == 0) {
        sentenceBegin = before;
      }
      if (idw == words - 1) {
        sentenceEnd = before + word.size();
      }
    }
    if (debug) {
      std::cout << std::endl;
    }

    sentenceWords.push_back(wordByteRanges);
    sentenceByteRanges.push_back((ByteRange){sentenceBegin, sentenceEnd});
  }

  // Use the AnnotatedText's addSentence() API to add sentences to transparently
  // convert from string_views to ByteRanges, rebasing/working out the math
  // underneath.
  if (debug) {
    std::cout << "Inserting words onto container:" << std::endl;
  }

  ///
  std::vector<std::vector<marian::string_view>> wordStringViews;
  for (auto &sentence : sentenceWords) {
    std::vector<marian::string_view> wordByteRanges;
    for (auto &word : sentence) {
      marian::string_view wordView(&testAnnotation.text[word.begin],
                                   word.size());
      wordByteRanges.push_back(wordView);
      if (debug) {
        std::cout << std::string(wordView) << " ";
      }
    }
    testAnnotation.addSentence(wordByteRanges);
    wordStringViews.push_back(wordByteRanges);
    if (debug) {
      std::cout << std::endl;
    }
  }

  // Access from the sentence(sentenceIdx) API and confirm that the ground truth
  // we expect is same as what comes out of the contianer.
  std::vector<marian::string_view> sentenceStringViews;
  for (auto &sentenceByteRange : sentenceByteRanges) {
    marian::string_view sentenceView(
        &testAnnotation.text[sentenceByteRange.begin],
        sentenceByteRange.size());
    sentenceStringViews.push_back(sentenceView);
  }

  if (debug) {
    std::cout << "From container: " << std::endl;
    std::cout << "Sentences: " << std::endl;
  }
  for (int idx = 0; idx < sentenceByteRanges.size(); idx++) {
    ByteRange expected = sentenceByteRanges[idx];
    ByteRange obtained = testAnnotation.sentenceAsByteRange(idx);
    if (debug) {
      std::cout << std::string(testAnnotation.sentence(idx)) << " ";
    }
    CHECK(expected.begin == obtained.begin);
    CHECK(expected.end == obtained.end);
    std::string expected_string = std::string(sentenceStringViews[idx]);
    std::string obtained_string = std::string(testAnnotation.sentence(idx));
    CHECK(expected_string == obtained_string);
  }

  /// Access the word(sentenceIdx, wordIdx) API and confirm what we hold as
  /// expected words are the same as those obtained.
  if (debug) {
    std::cout << "From container: " << std::endl;
    std::cout << "Words: " << std::endl;
  }

  CHECK(sentenceWords.size() == testAnnotation.numSentences());
  for (int idx = 0; idx < sentenceWords.size(); idx++) {
    CHECK(sentenceWords[idx].size() == testAnnotation.numWords(idx));
  }

  for (int idx = 0; idx < sentenceWords.size(); idx++) {
    for (int idw = 0; idw < sentenceWords[idx].size(); idw++) {
      ByteRange expected = sentenceWords[idx][idw];
      ByteRange obtained = testAnnotation.wordAsByteRange(idx, idw);
      if (debug) {
        std::cout << std::string(testAnnotation.word(idx, idw)) << " ";
      }
      CHECK(expected.begin == obtained.begin);
      CHECK(expected.end == obtained.end);

      std::string expected_string = std::string(wordStringViews[idx][idw]);
      std::string obtained_string = std::string(testAnnotation.word(idx, idw));
      CHECK(expected_string == obtained_string);
    }
    if (debug) {
      std::cout << std::endl;
    }
  }
}
