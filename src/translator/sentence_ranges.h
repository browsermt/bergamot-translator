#ifndef BERGAMOT_SENTENCE_RANGES_H_
#define BERGAMOT_SENTENCE_RANGES_H_

#include "data/types.h"
#include <cassert>
#include <vector>

namespace marian {
namespace bergamot {

template <class string_view_type> class SentenceRangesT {
  // SentenceRanges stores string_view_types into a source text, with additional
  // annotations to mark sentence boundaries.
  //
  // Given the availability annotations, this container provides capabilty to
  // add sentences, and access individual sentences.
public:
  typedef typename std::vector<string_view_type>::iterator WordIterator;

  void addSentence(std::vector<string_view_type> &wordRanges) {
    addSentence(std::begin(wordRanges), std::end(wordRanges));
  };
  void addSentence(WordIterator begin, WordIterator end) {

    size_t size = flatByteRanges_.size();
    flatByteRanges_.insert(std::end(flatByteRanges_), begin, end);
    sentenceBeginIds_.push_back(size);
  };

  void clear() {
    flatByteRanges_.clear();
    sentenceBeginIds_.clear();
  }

  size_t numSentences() const { return sentenceBeginIds_.size(); }

  // Returns a string_view_type into the ith sentence.
  string_view_type sentence(size_t index) const {
    size_t bos_id;
    string_view_type eos, bos;

    bos_id = sentenceBeginIds_[index];
    bos = flatByteRanges_[bos_id];

    if (index + 1 == numSentences()) {
      eos = flatByteRanges_.back();
    } else {
      assert(index < numSentences());
      size_t eos_id = sentenceBeginIds_[index + 1];
      --eos_id;
      eos = flatByteRanges_[eos_id];
    }

    return sentenceBetween(bos, eos);
  };

  void words(size_t index, std::vector<string_view_type> &out) {
    wordsT<string_view_type>(index, out);
  }

  template <class tgt_string_view_type>
  void wordsT(size_t index, std::vector<tgt_string_view_type> &out) {
    for (size_t idx = sentenceBeginIds_[index];
         idx < sentenceBeginIds_[index + 1]; idx++) {
      out.emplace_back(flatByteRanges_[idx].data(),
                       flatByteRanges_[idx].size());
    }
  }

  SentenceRangesT() {}

  template <class src_type>
  SentenceRangesT(const SentenceRangesT<src_type> &other) {
    sentenceBeginIds_ = other.sentenceBeginIds();
    for (auto &sview : other.flatByteRanges()) {
      flatByteRanges_.emplace_back(sview.data(), sview.size());
    }
  };

  const std::vector<string_view_type> &flatByteRanges() const {
    return flatByteRanges_;
  }

  const std::vector<size_t> &sentenceBeginIds() const {
    return sentenceBeginIds_;
  }

private:
  // A flat storage for string_view_types. Can be words or sentences.
  std::vector<string_view_type> flatByteRanges_;

  // The container grows dynamically with addSentence. size_t marking index is
  // used to ensure the sentence boundaries stay same while underlying storage
  // might be changed during reallocation.
  std::vector<size_t> sentenceBeginIds_;

  // Utility function to extract the string starting at firstWord and ending at
  // lastWord as a single string-view.
  string_view_type sentenceBetween(string_view_type firstWord,
                                   string_view_type lastWord) const {

    const char *data = firstWord.data();
    size_t size = lastWord.data() + lastWord.size() - firstWord.data();
    return string_view_type(data, size);
  };
};

typedef SentenceRangesT<string_view> SentenceRanges;

/// An AnnotatedBlob is blob std::string along with the annotation which are
/// valid until the original blob is valid (string shouldn't be invalidated).
/// Annotated Blob, due to its nature binding a blob to string_view should only
/// be move-constructible as a whole.
template <class Annotation> struct AnnotatedBlobT {
  std::string blob;
  SentenceRangesT<Annotation> annotation;
  AnnotatedBlobT(){};

  template <class src_annotation_type>
  AnnotatedBlobT(std::string &&blob,
                 SentenceRangesT<src_annotation_type> &&annotation)
      : blob(std::move(blob)), annotation(annotation) {}

  AnnotatedBlobT(std::string &&blob, SentenceRangesT<Annotation> &&annotation)
      : blob(std::move(blob)), annotation(std::move(annotation)) {}

  template <class src_annotation_type>
  AnnotatedBlobT(AnnotatedBlobT<src_annotation_type> &&other)
      : blob(std::move(other.blob)), annotation(std::move(other.annotation)) {}

  AnnotatedBlobT(const AnnotatedBlobT &other) = delete;
  AnnotatedBlobT &operator=(const AnnotatedBlobT &other) = delete;
  const size_t numSentences() const { return annotation.numSentences(); }
};

} // namespace bergamot

} // namespace marian

#endif //  BERGAMOT_SENTENCE_RANGES_H_
