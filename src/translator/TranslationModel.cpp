/*
 * TranslationModel.cpp
 *
 */

#include <future>
#include <vector>

// All local project includes
#include "TranslationModel.h"
#include "translator/parser.h"
#include "translator/response.h"
#include "translator/service.h"

TranslationModel::TranslationModel(const std::string &config,
                                   marian::bergamot::AlignedMemory model_memory,
                                   marian::bergamot::AlignedMemory lexical_memory)
    : service_(config, std::move(model_memory), std::move(lexical_memory)) {}

TranslationModel::~TranslationModel() {}

std::vector<TranslationResult>
TranslationModel::translate(std::vector<std::string> &&texts,
                            TranslationRequest request) {

  // This code, move into async?
  std::vector<TranslationResult> translationResults;
  std::vector<marian::bergamot::Response> responses =
      service_.translateMultiple(std::move(texts), request);
  for (auto &response : responses) {
    TranslationResult::SentenceMappings sentenceMappings;
    for (size_t idx = 0; idx < response.size(); idx++) {
      marian::string_view src = response.source.sentence(idx);
      marian::string_view tgt = response.target.sentence(idx);
      sentenceMappings.emplace_back(std::string_view(src.data(), src.size()),
                                    std::string_view(tgt.data(), tgt.size()));
    }

    // In place construction.
    translationResults.emplace_back(
        std::move(response.source.text), // &&response.source_
        std::move(response.target.text), // &&response.translation_
        std::move(sentenceMappings)      // &&sentenceMappings
    );
  }

  return translationResults;
}

bool TranslationModel::isAlignmentSupported() const { return false; }
