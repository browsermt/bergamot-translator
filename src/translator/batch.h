#ifndef SRC_BERGAMOT_BATCH_H
#define SRC_BERGAMOT_BATCH_H

#include "request.h"
#include "translator/beam_search.h"

namespace marian {
namespace bergamot {

// An empty batch is poison.
class Batch {
 public:
  Batch() {}
  void clear() { sentences_.clear(); }

  size_t size() const { return sentences_.size(); }

  void add(const RequestSentence &sentence);

  // Accessors to read from a Batch. For use in BatchTranslator (consumer on a
  // PCQueue holding batches).
  //
  // sentences() are used to access sentences to construct marian internal
  // batch.
  const RequestSentences &sentences() { return sentences_; }

  // On obtaining Histories after translating a batch, completeBatch can be
  // called with Histories , which forwards the call to Request through
  // RequestSentence and triggers completion, by setting the promised value to
  // the future given to client.
  void completeBatch(const Histories &histories);

  // Convenience function to log batch-statistics. numTokens, max-length.
  void log();

 private:
  RequestSentences sentences_;
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_BATCH_H_
