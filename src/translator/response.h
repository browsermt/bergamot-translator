#ifndef SRC_BERGAMOT_RESPONSE_H_
#define SRC_BERGAMOT_RESPONSE_H_

#include "sentence_ranges.h"
#include "data/types.h"
#include "definitions.h"
#include "translator/beam_search.h"

#include <cassert>
#include <string>
#include <vector>

namespace marian {
namespace bergamot {
class Response {
  // Response is a marian internal class (not a bergamot-translator class)
  // holding source blob of text, vector of TokenRanges corresponding to each
  // sentence in the source text blob and histories obtained from translating
  // these sentences.
  //
  // This class provides an API at a higher level in comparison to History to
  // access translations and additionally use string_view manipulations to
  // recover structure in translation from source-text's structure known through
  // reference string and string_view. As many of these computations are not
  // required until invoked, they are computed as required and stored in data
  // members where it makes sense to do so (translation,translationTokenRanges).
  //
  // Examples of such use-cases are:
  //    translation()
  //    translationInSourceStructure() TODO(@jerinphilip)
  //    alignment(idx) TODO(@jerinphilip)
  //    sentenceMappings (for bergamot-translator)

public:
  Response(std::string &&source, SentenceRanges &&sourceRanges,
           Histories &&histories,
           // Required for constructing translation and TokenRanges within
           // translation lazily.
           std::vector<Ptr<Vocab const>> &vocabs);

  // Move constructor.
  Response(Response &&other)
      : source_(std::move(other.source_)),
        translation_(std::move(other.translation_)),
        sourceRanges_(std::move(other.sourceRanges_)),
        targetRanges_(std::move(other.targetRanges_)),
        histories_(std::move(other.histories_)),
        vocabs_(std::move(other.vocabs_)){};

  // Prevents CopyConstruction and CopyAssignment. sourceRanges_ is constituted
  // by string_view and copying invalidates the data member.
  Response(const Response &) = delete;
  Response &operator=(const Response &) = delete;

  typedef std::vector<std::pair<const string_view, const string_view>>
      SentenceMappings;

  // Moves source sentence into source, translated text into translation.
  // Pairs of string_views to corresponding sentences in
  // source and translation are loaded into sentenceMappings. These string_views
  // reference the new source and translation.
  //
  // Calling move() invalidates the Response object as ownership is transferred.
  // Exists for moving strc
  void move(std::string &source, std::string &translation,
            SentenceMappings &sentenceMappings);

  const Histories &histories() const { return histories_; }
  const std::string &source() const { return source_; }
  const std::string &translation() {
    constructTranslation();
    return translation_;
  }

  // A convenience function provided to return translated text placed within
  // source's structure. This is useful when the source text is a multi-line
  // paragraph or string_views extracted from structured text like HTML and it's
  // desirable to place the individual sentences in the locations of the source
  // sentences.
  // const std::string translationInSourceStructure();
  // const PendingAlignmentType alignment(size_t idx);

private:
  void constructTranslation();
  void constructSentenceMappings(SentenceMappings &);

  std::string source_;
  SentenceRanges sourceRanges_;
  Histories histories_;

  std::vector<Ptr<Vocab const>> *vocabs_;
  bool translationConstructed_{false};
  std::string translation_;
  SentenceRanges targetRanges_;
};
} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_RESPONSE_H_
