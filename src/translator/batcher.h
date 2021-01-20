#ifndef SRC_BERGAMOT_BATCHER_H_
#define SRC_BERGAMOT_BATCHER_H_

#include "common/options.h"
#include "data/corpus_base.h"
#include "definitions.h"
#include "request.h"

#include <set>
#include <vector>

namespace marian {
namespace bergamot {
class Batcher {
public:
  explicit Batcher(Ptr<Options> options);

  // RequestSentence incorporates (tentative) notions of priority with each
  // sentence. This method inserts the sentence into the internal data-structure
  // which maintains priority among sentences from multiple concurrent requests.
  void addSentenceWithPriority(RequestSentence &sentence);

  // Loads sentences with sentences compiled from (tentatively) multiple
  // requests optimizing for both padding and priority.
  void cleaveBatch(RequestSentences &sentences);

private:
  unsigned int max_input_tokens_;
  std::vector<std::set<RequestSentence>> bucket_;
};

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_BATCHER_H_
