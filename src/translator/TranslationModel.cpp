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
                                   marian::bergamot::AlignedMemory model_memory,
                                   marian::bergamot::AlignedMemory lexical_memory)
    : service_(config, std::move(model_memory), std::move(lexical_memory)) {}

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

    TranslationResult::SentenceMappings sentenceMappings;
    for (size_t idx = 0; idx < marianResponse.size(); idx++) {
      marian::string_view src = marianResponse.source.sentence(idx);
      marian::string_view tgt = marianResponse.target.sentence(idx);
      sentenceMappings.emplace_back(std::string_view(src.data(), src.size()),
                                    std::string_view(tgt.data(), tgt.size()));
    }

    // In place construction.
    translationResults.emplace_back(
        std::move(marianResponse.source.text), // &&marianResponse.source_
        std::move(marianResponse.target.text), // &&marianResponse.translation_
        std::move(sentenceMappings)            // &&sentenceMappings
    );
  }

  return translationResults;
}

bool TranslationModel::isAlignmentSupported() const { return false; }
