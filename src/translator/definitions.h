#ifndef SRC_BERGAMOT_DEFINITIONS_H_
#define SRC_BERGAMOT_DEFINITIONS_H_

#include "data/types.h"
#include "data/vocab_base.h"
#include "aligned.h"
#include <vector>

namespace marian {
namespace bergamot {

typedef marian::Words Segment;
typedef std::vector<Segment> Segments;

/// Shortcut to AlignedVector<char> for byte arrays
typedef AlignedVector<char> AlignedMemory;

/// Bundles all byte-array memories
struct MemoryBundle {
  AlignedMemory                                model;
  AlignedMemory                                shortlist;
  std::vector<std::shared_ptr<AlignedMemory>>  vocabs;
  AlignedMemory                                ssplitPrefixFile;

  MemoryBundle() = default;

  MemoryBundle(MemoryBundle &&from){
    model = std::move(from.model);
    shortlist = std::move(from.shortlist);
    vocabs = std::move(vocabs);
    ssplitPrefixFile = std::move(from.ssplitPrefixFile);
  }

  MemoryBundle &operator=(MemoryBundle &&from) {
    model = std::move(from.model);
    shortlist = std::move(from.shortlist);
    vocabs = std::move(vocabs);
    ssplitPrefixFile = std::move(from.ssplitPrefixFile);
    return *this;
  }

  // Delete copy constructors
  MemoryBundle(const MemoryBundle&) = delete;
  MemoryBundle& operator=(const MemoryBundle&) = delete;
};

} // namespace bergamot
} // namespace marian

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
} // namespace std
#else
#include <string_view>
#endif

#endif // SRC_BERGAMOT_DEFINITIONS_H_
