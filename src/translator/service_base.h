#ifndef SRC_BERGAMOT_SUBSTANDARD_SERVICE_H_
#define SRC_BERGAMOT_SUBSTANDARD_SERVICE_H_
#include "batch_translator.h"
#include "batcher.h"
#include "data/types.h"
#include "response.h"
#include "text_processor.h"

#include <queue>
#include <vector>

namespace marian {
namespace bergamot {

class ServiceBase {
public:
  explicit ServiceBase(Ptr<Options> options);
  std::future<Response> translateWithCopy(std::string input) {
    return translate(std::move(input));
  };

  std::future<Response> translate(std::string &&input);
  Ptr<Vocab const> sourceVocab() const { return vocabs_.front(); }
  Ptr<Vocab const> targetVocab() const { return vocabs_.back(); }
  virtual void enqueue() = 0;
  virtual void stop() = 0;

protected:
  size_t requestId_;
  std::vector<Ptr<Vocab const>> vocabs_; // ORDER DEPENDENCY
  TextProcessor text_processor_;         // ORDER DEPENDENCY
  Batcher batcher_;
};

class NonThreadedService : public ServiceBase {
public:
  explicit NonThreadedService(Ptr<Options> options);
  void enqueue() override;
  void stop() override{};

private:
  BatchTranslator translator_;
};

std::vector<Ptr<const Vocab>> loadVocabularies(Ptr<Options> options);

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_SUBSTANDARD_SERVICE_H_
