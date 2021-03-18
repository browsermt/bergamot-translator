#ifndef SRC_BERGAMOT_RESPONSE_H_
#define SRC_BERGAMOT_RESPONSE_H_

#include "data/alignment.h"
#include "data/types.h"
#include "definitions.h"
#include "sentence_ranges.h"
#include "translator/beam_search.h"

#include <cassert>
#include <string>
#include <vector>

namespace marian {
namespace bergamot {

typedef marian::data::SoftAlignment SoftAlignment;
typedef marian::data::WordAlignment WordAlignment;

/// Alignment is stored as a sparse matrix, this pretty much aligns with marian
/// internals but is brought here to maintain translator
/// agnosticism/independence.
struct Point {
  size_t src; // Index pointing to source string_view.
  size_t tgt; // Index pointing to target string_view.
  float prob; // Score between [0, 1] on indicating degree of alignment.
};

typedef std::vector<Point> Alignment;

struct Quality {
  float sequence; /// Certainty/uncertainty score for sequence.
  /// Certainty/uncertainty for each word in the sequence.
  std::vector<float> word;
};

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
  Response(AnnotatedBlob &&source, Histories &&histories,
           std::vector<Ptr<Vocab const>> &vocabs);

  // Move constructor.
  Response(Response &&other)
      : source(std::move(other.source)), target(std::move(other.target)),
        alignments(std::move(other.alignments)),
        qualityScores(std::move(other.qualityScores)),
        histories_(std::move(other.histories_)){};

  // Prevents CopyConstruction and CopyAssignment. sourceRanges_ is constituted
  // by string_view and copying invalidates the data member.
  Response(const Response &) = delete;
  Response &operator=(const Response &) = delete;

  const size_t size() const { return source.numSentences(); }
  const Histories &histories() const { return histories_; }

  AnnotatedBlob source;
  AnnotatedBlob target;
  std::vector<Quality> qualityScores;
  std::vector<Alignment> alignments;

  const std::string &translation() {
    LOG(info, "translation() will be deprecated now that target is public.");
    return target.blob;
  }

private:
  Histories histories_;
};
} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_RESPONSE_H_
