#ifndef SRC_BERGAMOT_PROCESSED_REQUEST_SENTENCE_H_
#define SRC_BERGAMOT_PROCESSED_REQUEST_SENTENCE_H_

#include <memory>
#include <string>
#include <vector>

#include "translator/beam_search.h"

namespace marian {
namespace bergamot {

/// History is marian object with plenty of `shared_ptr`s lying around, which makes keeping a lid on memory hard for
/// cache, due to copies being shallow rather than deep. We therefore process marian's lazy History management changing
/// to something eager, keeping only what we want and units accounted for. Each of these corresponds to a
/// `RequestSentence` and retains the minimum information required to reconstruct a full `Response` if cached.
///
/// L4 requires byte-streams, which makes easier to keep tight control over "memory" we need to have accounting for
/// cache-size based on memory/bytes as well.
///
class ProcessedRequestSentence {
 public:
  /// Constructs an empty (uninitialized) ProcessedRequestSentence
  ProcessedRequestSentence();

  /// Construct from History - consolidate members which we require further and store the complete object (not
  /// references to shared-pointers) for storage and recompute efficiency purposes.
  ProcessedRequestSentence(const History &history);

  // Helpers to work with blobs / bytearray storing the class, particularly for L4. Storage follows the order of member
  // definitions in this class. With vectors prefixed with sizes to allocate before reading in with the right sizes.

  /// Deserialize the contents of an instance from a sequence of bytes. Compatible with `toBytes`.
  static ProcessedRequestSentence fromBytes(char const *data, size_t size);

  /// Serialize the contents of an instance to a sequence of bytes. Should be compatible with `fromBytes`.
  std::string toBytes() const;

  void debug();

  // Const accessors for private members
  const marian::Words &words() const { return words_; }
  const data::SoftAlignment &softAlignment() const { return softAlignment_; }
  const std::vector<float> &wordScores() const { return wordScores_; }
  float sentenceScore() const { return sentenceScore_; }

 private:
  // Note: Adjust blob IO in order of the member definitions here, in the event of change.
  marian::Words words_;                ///< vector of marian::Word, no shared_ptrs
  data::SoftAlignment softAlignment_;  ///< history->traceSoftAlignment(); std::vector<std::vector<float>>
  std::vector<float> wordScores_;      ///< hyp->tracebackWordScores();
  float sentenceScore_;                ///< normalizedPathScore
};

typedef std::vector<std::unique_ptr<ProcessedRequestSentence>> ProcessedRequestSentences;

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_PROCESSED_REQUEST_SENTENCE_H_
