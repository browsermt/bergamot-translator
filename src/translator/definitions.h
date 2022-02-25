#ifndef SRC_BERGAMOT_DEFINITIONS_H_
#define SRC_BERGAMOT_DEFINITIONS_H_

#include <vector>

#include "aligned.h"
#include "data/types.h"
#include "data/vocab_base.h"

namespace marian {
namespace bergamot {

typedef marian::Words Segment;
typedef std::vector<Segment> Segments;

/// Shortcut to AlignedVector<char> for byte arrays
typedef AlignedVector<char> AlignedMemory;

/// Memory bundle for all byte-arrays.
/// Can be a set/subset of model, shortlist, vocabs and ssplitPrefixFile bytes.
struct MemoryBundle {
  AlignedMemory model{};      ///< Byte-array of model (aligned to 256)
  AlignedMemory shortlist{};  ///< Byte-array of shortlist (aligned to 64)

  /// Vector of vocabulary memories (aligned to 64).
  /// If two vocabularies are the same (based on the filenames), two entries (shared
  /// pointers) will be generated which share the same AlignedMemory object.
  std::vector<std::shared_ptr<AlignedMemory>> vocabs{};

  /// @todo Not implemented yet
  AlignedMemory ssplitPrefixFile{};

  AlignedMemory qualityEstimatorMemory;  ///< Byte-array of qe model (aligned to 64)
};

/// ByteRange stores indices for half-interval [begin, end) in a string. Can be
/// used to represent a sentence, word.
struct ByteRange {
  size_t begin;
  size_t end;
  const size_t size() const { return end - begin; }
  bool operator==(ByteRange other) const { return begin == other.begin && end == other.end; }
};

/// A Subword range is mechanically the same as a `ByteRange`, but instead of
/// describing a span of bytes, it describes a span of Subword tokens. Using
/// `Annotation.word()` you can switch between the two.
struct SubwordRange {
  size_t begin;
  size_t end;
  const size_t size() const { return end - begin; }
  bool operator==(SubwordRange other) const { return begin == other.begin && end == other.end; }
};

class Response;
using CallbackType = std::function<void(Response &&)>;

}  // namespace bergamot
}  // namespace marian

// @TODO at the moment the usage of string_view in this repository is a hot mess and a disaster waiting to happen.
// ssplit uses std::string_view if the compiler supports c++17, else falls back to c++11 and absl::string_view
// bergamot-translator uses, depending on the source file std::string_view (which will break if ssplit-cpp uses
// absl::string_view) and marian::string_view which is an export of (confusingly) the sentencepiece module that
// marian has. marian::string_view is our addition to the marian fork, which will make merging even nicer. Not.
// This is just an ugly patchwork that allos gcc5, our lowest targetted gcc to run. We don't actually try to run
// on older compilers.

#if defined(__GNUC__) && __GNUC__ < 6 && !defined(__clang__)
#include <experimental/string_view>
namespace std {
using string_view = std::experimental::string_view;
}  // namespace std
#else
#include <string_view>
#endif

#endif  // SRC_BERGAMOT_DEFINITIONS_H_
