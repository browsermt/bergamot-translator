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

typedef SentenceRangesT<string_view> SentenceRanges;
typedef AnnotatedBlobT<string_view> AnnotatedBlob;

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
      : source(std::move(other.source)), target(std::move(other.target)),
        histories_(std::move(other.histories_)),
        vocabs_(std::move(other.vocabs_)){};

  // Prevents CopyConstruction and CopyAssignment. sourceRanges_ is constituted
  // by string_view and copying invalidates the data member.
  Response(const Response &) = delete;
  Response &operator=(const Response &) = delete;

  const Histories &histories() const { return histories_; }

  AnnotatedBlob source;
  AnnotatedBlob target;

  const std::string &translation() {
    LOG(info, "translation() will be deprecated now that target is public.");
    return target.blob;
  }

private:
  Histories histories_;
  std::vector<Ptr<Vocab const>> *vocabs_;
};
} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_RESPONSE_H_
