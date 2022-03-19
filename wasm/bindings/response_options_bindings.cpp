/*
 * Bindings for ResponseOptions class
 *
 */

#include <emscripten/bind.h>

#include <sstream>
#include <string>
#include <unordered_set>

#include "html.h"
#include "response_options.h"

using ResponseOptions = marian::bergamot::ResponseOptions;

using HTMLOptions = std::optional<marian::bergamot::HTML::Options>;
using HTML = marian::bergamot::HTML;
using TagNameSet = HTML::TagNameSet;

using namespace emscripten;

void split(std::string const &str, char delimiter, TagNameSet &out) {
  size_t pos{0}, offset{0};
  while ((pos = str.find(delimiter, offset)) != std::string::npos) {
    if (pos > offset) out.emplace(str.substr(offset, pos - offset));
    offset = pos + 1;
  }
  if (offset != str.size()) out.emplace(str.substr(offset));
}

std::string join(TagNameSet const &items, char delimiter) {
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

auto getter(TagNameSet HTML::Options::*field) {
  return std::function([field](HTMLOptions &options) {
    if (!options.has_value()) options.emplace();
    return join(options.value().*field, ',');
  });
}

auto setter(TagNameSet HTML::Options::*field) {
  return std::function([field](HTMLOptions &options, std::string const &value) {
    if (!options.has_value()) options.emplace();
    (options.value().*field).clear();
    split(value, ',', options.value().*field);
  });
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
      .function("getVoidTags", getter(&HTML::Options::voidTags))
      .function("setVoidTags", setter(&HTML::Options::voidTags))
      .function("getInlineTags", getter(&HTML::Options::inlineTags))
      .function("setInlineTags", setter(&HTML::Options::inlineTags))
      .function("getInWordTags", getter(&HTML::Options::inWordTags))
      .function("setInWordTags", setter(&HTML::Options::inWordTags))
      .function("getIgnoredTags", getter(&HTML::Options::ignoredTags))
      .function("setIgnoredTags", setter(&HTML::Options::ignoredTags))
      .function("getContinuationDelimiters", &getContinuationDelimiters)
      .function("setContinuationDelimiters", &setContinuationDelimiters)
      .function("getSubstituteInlineTagsWithSpaces", &getSubstituteInlineTagsWithSpaces)
      .function("setSubstituteInlineTagsWithSpaces", &setSubstituteInlineTagsWithSpaces);
  register_vector<ResponseOptions>("VectorResponseOptions");
}
