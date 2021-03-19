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

/// Alignment is stored as a sparse matrix, this pretty much aligns with marian
/// internals but is brought here to maintain translator
/// agnosticism/independence.
struct Point {
  size_t src; // Index pointing to source ByteRange
  size_t tgt; // Index pointing to target ByteRange
  float prob; // Score between [0, 1] on indicating degree of alignment.
};

typedef std::vector<Point> Alignment;

/// -loglikelhoods of the sequence components as proxy to quality.
struct Quality {
  /// Certainty/uncertainty score for sequence.
  float sequence;
  /// Certainty/uncertainty for each word in the sequence.
  std::vector<float> word;
};

class Response {
  // Response is a marian internal class (not a bergamot-translator class)
  // holding annotated source blob of text, and translated blob of text,
  // alignment information between source and target words and sentences.
  // Annotations are markings of word and sentences boundaries represented using
  // ByteRanges.

public:
  Response(AnnotatedBlob &&source, Histories &&histories,
           std::vector<Ptr<Vocab const>> &vocabs);

  // Move constructor.
  Response(Response &&other)
      : source(std::move(other.source)), target(std::move(other.target)),
        alignments(std::move(other.alignments)),
        qualityScores(std::move(other.qualityScores)),
        histories_(std::move(other.histories_)){};

  // The following copy bans are not stricitly required anymore since Annotation
  // is composed of the ByteRange primitive (which was previously string_view
  // and required to be bound to string), but makes movement efficient by
  // banning these letting compiler complain about copies.
  Response(const Response &) = delete;
  Response &operator=(const Response &) = delete;

  const size_t size() const { return source.numSentences(); }

  AnnotatedBlob source; /// source-text, source.blob contains source
                        /// text. source.annotation holds the annotation.
  AnnotatedBlob target; /// translated-text, target.blob contains translated
                        /// text. target.annotation holds the annotation.
  std::vector<Quality>
      qualityScores; /// -logProb of each word and negative log likelihood
                     /// of sequence (sentence), for each sentence processed by
                     /// the translator. Indices correspond to ranges accessible
                     /// through annotation
  std::vector<Alignment>
      alignments; /// Alignments between source and target. Each Alignment is a
                  /// sparse matrix representation with indices corresponding
                  /// to ranges accessible through Annotations.

  const Histories &histories() const { return histories_; }

private:
  Histories histories_;
};
} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_RESPONSE_H_
