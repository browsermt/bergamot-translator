/*
 * TranslationModel.cpp
 *
 */

#include <future>
#include <vector>

// All local project includes
#include "TranslationModel.h"
#include "translator/parser.h"
#include "translator/service.h"

TranslationModel::TranslationModel(const std::string &config,
                                   const void *model_memory)
    : AbstractTranslationModel(), service_(config, model_memory) {}

TranslationModel::~TranslationModel() {}

std::vector<TranslationResult>
TranslationModel::translate(std::vector<std::string> &&texts,
                            TranslationRequest request) {
  // Implementing a non-async version first. Unpleasant, but should work.
  std::promise<std::vector<TranslationResult>> promise;
  auto future = promise.get_future();

  // This code, move into async?
  std::vector<TranslationResult> translationResults;
  for (auto &text : texts) {
    // Collect future as marian::bergamot::TranslationResult
    auto intermediate = service_.translate(std::move(text));
    intermediate.wait();
    auto marianResponse(std::move(intermediate.get()));

    // This mess because marian::string_view != std::string_view
    std::string source, translation;
    marian::bergamot::Response::SentenceMappings mSentenceMappings;
    marianResponse.move(source, translation, mSentenceMappings);

    // Convert to UnifiedAPI::TranslationResult
    TranslationResult::SentenceMappings sentenceMappings;
    for (auto &p : mSentenceMappings) {
      std::string_view src(p.first.data(), p.first.size()),
          tgt(p.second.data(), p.second.size());
      sentenceMappings.emplace_back(src, tgt);
    }

    // In place construction.
    translationResults.emplace_back(
        std::move(source),          // &&marianResponse.source_
        std::move(translation),     // &&marianResponse.translation_
        std::move(sentenceMappings) // &&sentenceMappings
    );
  }

  return translationResults;
}

bool TranslationModel::isAlignmentSupported() const { return false; }
