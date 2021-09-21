#ifndef SRC_BERGAMOT_AGGREGATE_BATCHING_POOL_H_
#define SRC_BERGAMOT_AGGREGATE_BATCHING_POOL_H_

#include <memory>
#include <queue>

#include "data/types.h"
#include "translation_model.h"

namespace marian {
namespace bergamot {

/// Hashes a pointer to an object using the address the pointer points to. If two pointers point to the same address,
/// they hash to the same value.  Useful to put widely shared_ptrs of entities (eg: TranslationModel, Vocab, Shortlist)
/// etc into containers which require the members to be hashable (std::unordered_set, std::unordered_map).
template <class T>
struct HashPtr {
  size_t operator()(const std::shared_ptr<T>& t) const {
    size_t address = reinterpret_cast<size_t>(t.get());
    return std::hash<size_t>()(address);
  }
};

/// Aggregates request queueing and generation of batches from multiple TranslationModels (BatchingPools within,
/// specifically), thereby acting as an intermediary to enable multiple translation model capability in BlockingService
/// and AsyncService.
///
/// A simple queue containing shared owning references to TranslationModels are held here from which batches are
/// generated on demand. Since a queue is involved, the ordering is first-come first serve on requests except there are
/// leaks effectively doing priority inversion if an earlier request with the same TranslationModel is pending
/// to be consumed for translation.
//
/// Actual storage for the request and batch generation are within the respective TranslationModels, which owns its own
/// BatchingPool.
///
/// Matches API provided by BatchingPool except arguments additionally parameterized by TranslationModel.
///
/// Note: This class is not thread-safe. You may use this class wrapped with ThreadsafeBatchingPool for a thread-safe
/// equivalent of this class, if needed.
class AggregateBatchingPool {
 public:
  /// Create an AggregateBatchingPool with (tentatively) global (across all BatchingPools) limits
  /// imposed here.
  AggregateBatchingPool();

  /// Enqueue an existing request onto model, also keep account of that this model and request are now pending.
  ///
  /// @param [in] model: Model to use in translation. A shared ownership to this model is accepted by this object to
  /// keep the model alive until translation is complete.
  /// @param [in] request: A request to be enqueued to model.
  /// @returns number of sentences added for translation.
  size_t enqueueRequest(Ptr<TranslationModel> model, Ptr<Request> request);

  /// Generate a batch from pending requests, obtained from available TranslationModels.
  ///
  /// @param [out] model: TranslationModel
  /// @param [out] batch: Batch to write onto, which is consumed at translation elsewhere.
  /// @returns Number of sentences in the generated batch.
  size_t generateBatch(Ptr<TranslationModel>& model, Batch& batch);

 private:
  std::unordered_set<std::shared_ptr<TranslationModel>, HashPtr<TranslationModel>> aggregateQueue_;
};

}  // namespace bergamot
}  // namespace marian

#endif  //  SRC_BERGAMOT_AGGREGATE_BATCHING_POOL_H_
