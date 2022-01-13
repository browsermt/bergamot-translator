/*
 * Bindings for ResponseOptions class
 *
 */

#include <emscripten/bind.h>

#include <sstream>

#include "html.h"
#include "response_options.h"

using ResponseOptions = marian::bergamot::ResponseOptions;

using HTMLOptions = std::optional<marian::bergamot::HTML::Options>;

using namespace emscripten;

void split(std::string const &str, char delimiter, std::unordered_set<std::string> &out) {
  std::string::size_type pos{0}, offset{0};
  while ((pos = str.find(delimiter, offset)) != std::string::npos) {
    if (pos > offset) out.emplace(str.substr(offset, pos - offset));
    offset = pos + 1;
  }
  if (offset != str.size()) out.emplace(str.substr(offset));
}

std::string join(std::unordered_set<std::string> const &items, char delimiter) {
  std::stringstream out;
  bool first = true;
  for (auto &&item : items) {
    if (first)
      first = false;
    else
      out << delimiter;
    out << item;
  }
  return out.str();
}

std::string getVoidTags(HTMLOptions &options) {
  if (!options.has_value()) options.emplace();
  return join(options->voidTags, ',');
}

void setVoidTags(HTMLOptions &options, std::string const &tags) {
  if (!options.has_value()) options.emplace();
  options->voidTags.clear();
  split(tags, ',', options->voidTags);
}

std::string getInlineTags(HTMLOptions &options) {
  if (!options.has_value()) options.emplace();
  return join(options->inlineTags, ',');
}

void setInlineTags(HTMLOptions &options, std::string const &tags) {
  if (!options.has_value()) options.emplace();
  options->inlineTags.clear();
  split(tags, ',', options->inlineTags);
}

std::string getContinuationDelimiters(HTMLOptions &options) {
  if (!options.has_value()) options.emplace();
  return options->continuationDelimiters;
}

void setContinuationDelimiters(HTMLOptions &options, std::string const &delimiters) {
  if (!options.has_value()) options.emplace();
  options->continuationDelimiters = delimiters;
}

bool getSubstituteInlineTagsWithSpaces(HTMLOptions &options) {
  if (!options.has_value()) options.emplace();
  return options->substituteInlineTagsWithSpaces;
}

void setSubstituteInlineTagsWithSpaces(HTMLOptions &options, bool enable) {
  if (!options.has_value()) options.emplace();
  options->substituteInlineTagsWithSpaces = enable;
}

EMSCRIPTEN_BINDINGS(response_options) {
  value_object<ResponseOptions>("ResponseOptions")
      .field("qualityScores", &ResponseOptions::qualityScores)
      .field("alignment", &ResponseOptions::alignment)
      .field("html", &ResponseOptions::HTML)
      .field("htmlOptions", &ResponseOptions::HTMLOptions);
}

EMSCRIPTEN_BINDINGS(html_options) {
  class_<HTMLOptions>("HTMLOptions")
      .constructor<>()
      .function("getVoidTags", &getVoidTags)
      .function("setVoidTags", &setVoidTags)
      .function("getInlineTags", &getInlineTags)
      .function("setInlineTags", &setInlineTags)
      .function("getContinuationDelimiters", &getContinuationDelimiters)
      .function("setContinuationDelimiters", &setContinuationDelimiters)
      .function("getSubstituteInlineTagsWithSpaces", &getSubstituteInlineTagsWithSpaces)
      .function("setSubstituteInlineTagsWithSpaces", &setSubstituteInlineTagsWithSpaces);
}
