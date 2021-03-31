#ifndef SRC_BERGAMOT_RESPONSE_H_
#define SRC_BERGAMOT_RESPONSE_H_

#include "data/types.h"
#include "definitions.h"
#include "sentence_ranges.h"
#include "translator/beam_search.h"

#include <cassert>
#include <string>
#include <vector>

namespace marian {
namespace bergamot {
class Response {
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
        sentenceMappings_(std::move(other.sentenceMappings_)),
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

  const std::string &source() const { return source_; }
  const std::string &translation() { return translation_; }
  const SentenceMappings sentenceMappings() const { return sentenceMappings_; }

  // A convenience function provided to return translated text placed within
  // source's structure. This is useful when the source text is a multi-line
  // paragraph or string_views extracted from structured text like HTML and it's
  // desirable to place the individual sentences in the locations of the source
  // sentences.
  // const std::string translationInSourceStructure();
  // const PendingAlignmentType alignment(size_t idx);

private:
  std::string source_;
  SentenceRanges sourceRanges_;

  std::vector<Ptr<Vocab const>> *vocabs_;
  std::string translation_;
  SentenceRanges targetRanges_;
  SentenceMappings sentenceMappings_;
};
} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_RESPONSE_H_
