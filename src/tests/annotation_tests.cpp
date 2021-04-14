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
  size_t sentences = 500;
  size_t maxWords = 40;

  // Set in case needed to see output. The output is in lines of #sentences +
  // header, which can be split and compared for easy understanding. The ideal
  // way to inspect what is going wrong is to redirect output and use to split
  // the different stages by sentences + 1 lines and check the diff.
  bool debug{false};

  std::mt19937 randomIntGen_;
  randomIntGen_.seed(42);

  AnnotatedText testAnnotation; // This the container we add through API and
                                // check if the access is correct.

  // External book-keeping so we have ground truths. Each element represents a
  // sentence.

  // word byte ranges - for testAnnotation.word(sId, wId)
  std::vector<std::vector<ByteRange>> groundTruthWords;
  // sentence byte ranges - for testAnnotation.sentence(sId, wId)
  std::vector<ByteRange> groundTruthSentences;

  // Prepare the text and construct ByteRanges as intended for sentences and
  // words. The ByteRanges we construct here are expected to be the
  // ground-truths for words and sentences. The string being constructed is like
  // as follows:
  //
  //     0-0 0-1 0-2 0-3
  //     1-0 1-1 1-2 1-3 1-4
  //     2-0 2-1
  //
  //     4-0 4-1 4-2 4-3
  //
  // Words are separated by space units.
  //
  // Below, we accumulate the text with intended structure as above, and
  // ground-truth tables populated to be aware of the ByteRanges where they are
  // meant to be.
  if (debug) {
    std::cout << "Preparing text and ground truth-tables" << std::endl;
  }
  for (size_t idx = 0; idx < sentences; idx++) {
    if (idx != 0)
      testAnnotation.text += "\n";

    // Words can be zero, we need to support empty word sentences as well.
    size_t numWords = randomIntGen_() % maxWords;

    std::vector<ByteRange> wordByteRanges;
    wordByteRanges.reserve(numWords);

    // For empty sentence, we expect it to be empty and marked in position where
    // the existing string is if needed to be pointed out.
    size_t before = testAnnotation.text.size() - 1;
    size_t sentenceBegin{before}, sentenceEnd{before};

    for (size_t idw = 0; idw < numWords; idw++) {
      if (idw != 0) {
        testAnnotation.text += " ";
        if (debug) {
          std::cout << " ";
        }
      }

      // Get new beginning, accounting for space above.
      before = testAnnotation.text.size();

      // Add the word
      std::string word = std::to_string(idx) + "-" + std::to_string(idw);
      testAnnotation.text += word;

      // Do math, before, before + new-word's size.
      wordByteRanges.push_back((ByteRange){before, before + word.size()});

      if (debug) {
        std::cout << word;
      }

      if (idw == 0) {
        sentenceBegin = before;
      }
      if (idw == numWords - 1) {
        sentenceEnd = before + word.size();
      }
    }
    if (debug) {
      std::cout << std::endl;
    }

    groundTruthWords.push_back(wordByteRanges);
    groundTruthSentences.push_back((ByteRange){sentenceBegin, sentenceEnd});
  }

  // We prepare string_views now with the known ByteRanges and use the
  // string_view based AnnotatedText.addSentence(...) API to add sentences to
  // transparently convert from string_views to ByteRanges, rebasing/working out
  // the math underneath.

  if (debug) {
    std::cout << "Inserting words onto container and save ground-truth-table:"
              << std::endl;
  }

  std::vector<std::vector<marian::string_view>> wordStringViews;
  for (auto &sentence : groundTruthWords) {
    std::vector<marian::string_view> wordByteRanges;
    bool first{true};
    for (auto &word : sentence) {
      marian::string_view wordView(&testAnnotation.text[word.begin],
                                   word.size());
      wordByteRanges.push_back(wordView);
      if (debug) {
        if (first) {
          first = false;
        } else {
          std::cout << " ";
        }
        std::cout << std::string(wordView);
      }
    }
    testAnnotation.addSentence(wordByteRanges);
    wordStringViews.push_back(wordByteRanges);
    if (debug) {
      std::cout << std::endl;
    }
  }

  if (debug) {
    std::cout
        << "Inserting sentences onto container and save ground-truth-table"
        << std::endl;
  }
  std::vector<marian::string_view> sentenceStringViews;
  for (auto &sentenceByteRange : groundTruthSentences) {
    char *data = &(testAnnotation.text[sentenceByteRange.begin]);
    marian::string_view sentenceView(data, sentenceByteRange.size());
    sentenceStringViews.push_back(sentenceView);

    if (debug) {
      std::cout << sentenceView << std::endl;
    }
  }

  // Access from the sentence(sentenceIdx) API and confirm that the ground truth
  // we expect is same as what comes out of the container.
  if (debug) {
    std::cout << "From container: Sentences" << std::endl;
  }
  for (int idx = 0; idx < groundTruthSentences.size(); idx++) {
    ByteRange expected = groundTruthSentences[idx];
    ByteRange obtained = testAnnotation.sentenceAsByteRange(idx);
    if (debug) {
      std::cout << std::string(testAnnotation.sentence(idx)) << std::endl;
    }
    CHECK(expected.begin == obtained.begin);
    CHECK(expected.end == obtained.end);
    std::string expected_string = std::string(sentenceStringViews[idx]);
    std::string obtained_string = std::string(testAnnotation.sentence(idx));
    CHECK(expected_string == obtained_string);
  }

  /// Access the word(sentenceIdx, wordIdx) API and confirm what we hold as
  /// expected words are the same as those obtained from the container.
  if (debug) {
    std::cout << "From container: Words" << std::endl;
  }

  CHECK(groundTruthWords.size() == testAnnotation.numSentences());
  for (int idx = 0; idx < groundTruthWords.size(); idx++) {
    CHECK(groundTruthWords[idx].size() == testAnnotation.numWords(idx));
  }

  for (int idx = 0; idx < groundTruthWords.size(); idx++) {
    for (int idw = 0; idw < groundTruthWords[idx].size(); idw++) {
      ByteRange expected = groundTruthWords[idx][idw];
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

  // Try inserting an empty Sentence. This is ensuring we check for empty
  // Sentence if the random test above does not cover it for some reason.
  int emptySentenceIdx = sentences;
  std::vector<marian::string_view> emptySentence;
  testAnnotation.addSentence(emptySentence);

  // There are no words.
  CHECK(testAnnotation.numWords(emptySentenceIdx) == 0);

  // Empty sentence expected at output.
  std::string expectedEmptyString = "";
  marian::string_view emptyView = testAnnotation.sentence(emptySentenceIdx);
  std::string obtainedString = std::string(emptyView.data(), emptyView.size());
  CHECK(expectedEmptyString == obtainedString);
}
