#include "html_tests.h"

#include <vector>

#include "catch.hpp"
#include "data/types.h"  // for marian::string_view
#include "translator/html.h"
#include "translator/response.h"

using namespace marian::bergamot;
using marian::string_view;

std::ostream &operator<<(std::ostream &out, std::pair<ByteRange, ByteRange> const &b) {
  return out << '(' << b.first << ',' << b.second << ')';
}

std::ostream &operator<<(std::ostream &out, ByteRange const &b) { return out << '{' << b.begin << ',' << b.end << '}'; }

std::vector<ByteRange> AsByteRanges(AnnotatedText const &annotation) {
  std::vector<ByteRange> words;
  words.emplace_back(annotation.annotation.gap(0));
  for (size_t sentenceIdx = 0; sentenceIdx < annotation.numSentences(); ++sentenceIdx) {
    for (size_t wordIdx = 0; wordIdx < annotation.numWords(sentenceIdx); ++wordIdx)
      words.emplace_back(annotation.wordAsByteRange(sentenceIdx, wordIdx));
    words.emplace_back(annotation.annotation.gap(sentenceIdx + 1));
  }
  return words;
}

std::vector<std::string> AsTokens(AnnotatedText const &annotation) {
  std::vector<std::string> words;
  words.emplace_back(annotation.gap(0));
  for (size_t sentenceIdx = 0; sentenceIdx < annotation.numSentences(); ++sentenceIdx) {
    for (size_t wordIdx = 0; wordIdx < annotation.numWords(sentenceIdx); ++wordIdx)
      words.emplace_back(annotation.word(sentenceIdx, wordIdx));
    words.emplace_back(annotation.gap(sentenceIdx + 1));
  }
  return words;
}

void RecordSentenceFromByteRange(AnnotatedText &text, std::vector<ByteRange> const &ranges) {
  assert(ranges.size() > 0);

  std::vector<string_view> tokens;
  tokens.reserve(ranges.size());

  for (auto &&range : ranges) tokens.emplace_back(text.text.data() + range.begin, range.size());

  text.recordExistingSentence(tokens.begin(), tokens.end(), text.text.data() + ranges[0].begin);
}

TEST_CASE("Ignore HTML if process_markup is false") {
  std::string html_code("<p>This text &amp; has <b>HTML</b> in it</p>");

  std::string input(html_code);
  HTML html(std::move(input), false);
  CHECK(input == html_code);

  Response response;
  response.source.text = html_code;
  response.target.text = html_code;
  html.Restore(response);

  // Assert that Restore() does not mess with my HTML code
  CHECK(response.source.text == html_code);
}

TEST_CASE("Test reconstruction") {
  std::string input("<p><input>H<u>e</u>llo <b>world</b> how <u>are you</u>?</p>\n");

  std::string text(input);
  HTML html(std::move(text), true);  // TODO: move, but really a reference?
  CHECK(text == "Hello world how are you?\n");

  AnnotatedText source(std::move(text));
  std::vector<string_view> tokens{
      string_view(source.text.data() + 0, 4),   // Hell
      string_view(source.text.data() + 4, 1),   // o
      string_view(source.text.data() + 5, 6),   // _world
      string_view(source.text.data() + 11, 4),  // _how
      string_view(source.text.data() + 15, 4),  // _are
      string_view(source.text.data() + 19, 4),  // _you
      string_view(source.text.data() + 23, 1),  // ?
      string_view(source.text.data() + 24, 0),  // "\n" (but 0 length?)
  };

  source.recordExistingSentence(tokens.begin(), tokens.end(), source.text.data());

  Response response;
  response.source = source;

  html.Restore(response);
  // CHECK(response.source.text == input);  // fails because <u></u> has been moved to the front of the token
  CHECK(response.source.text == "<p><input><u></u>Hello <b>world</b> how <u>are you</u>?</p>\n");

  std::vector<ByteRange> restored_tokens{
      ByteRange{ 0, 0 +  0},  // (start of sentence)
      ByteRange{ 0, 0 + 21},  // <p><input>H<u>e</u>ll
      ByteRange{21, 21 + 1},  // o
      ByteRange{22, 22 + 9},  // _<b>world
      ByteRange{31, 31 + 8},  // </b>_how
      ByteRange{39, 39 + 7},  // _<u>are
      ByteRange{46, 46 + 4},  // _you
      ByteRange{50, 50 + 5},  // </u>?
      ByteRange{55, 55 + 0},  // ""
      ByteRange{55, 55 + 5},  // </p>\n
  };
  CHECK(response.source.text.size() == restored_tokens.back().end);
  CHECK(AsByteRanges(response.source) == restored_tokens);

  // Same test as above, but easier to read. Will use this further down.
  std::vector<std::string> restored_tokens_str{
      "",
      "<p><input><u></u>Hell",  // Should really be "<p><input>H<u>e</u>ll"
      "o",
      " <b>world",
      "</b> how",
      " <u>are",
      " you",
      "</u>?",
      "",       // end of sentence
      "</p>\n"
  };

  CHECK(AsTokens(response.source) == restored_tokens_str);
}

TEST_CASE("Test reconstruction of multiple sentences") {
  std::string input("<p>This <em>is a sentence. And so is</em> this.</p>\n");

  HTML html(std::move(input), true);
  CHECK(input == "This is a sentence. And so is this.\n");

  Response response;
  response.source = AnnotatedText(std::move(input));

  RecordSentenceFromByteRange(response.source, {
                                                   ByteRange{0, 4},    // 0.0 "This"
                                                   ByteRange{4, 7},    // 0.1 " is"
                                                   ByteRange{7, 9},    // 0.2 " a"
                                                   ByteRange{9, 18},   // 0.3 " sentence"
                                                   ByteRange{18, 19},  // 0.4 "."
                                               });

  RecordSentenceFromByteRange(response.source, {
                                                   ByteRange{20, 23},  // 1.0 "And"
                                                   ByteRange{23, 26},  // 1.1 " so"
                                                   ByteRange{26, 29},  // 1.2 " is"
                                                   ByteRange{29, 34},  // 1.3 " this"
                                                   ByteRange{34, 35},  // 1.4 "."
                                               });

  std::vector<std::string> tokens{"",    "This", " is", " a",    " sentence", ".", " ",
                                  "And", " so",  " is", " this", ".",         "\n"};

  CHECK(AsTokens(response.source) == tokens);

  html.Restore(response);

  std::vector<std::string> html_tokens{
      "",       "<p>This", " <em>is", " a", " sentence", ".", " ", "And", " so", " is", "</em> this", ".",
      "</p>\n",  // </p> got moved into post-sentence gap
  };

  CHECK(AsTokens(response.source) == html_tokens);
}

TEST_CASE("Test case html entities") {
  // These are all entities I would expect in innerHTML, since all other entities
  // can be encoded as UTF-8 so there's no need to encode them through &...; when
  // innerHTML encodes the DOM as HTML.
  std::string input("<p data-attr=\"&quot;&apos;\">This is a sentence &lt;with&gt; named &amp; entities</p>\n");
  HTML html(std::move(input), true);
  CHECK(input == "This is a sentence <with> named & entities\n");

  Response response;
  response.source = AnnotatedText(std::move(input));

  RecordSentenceFromByteRange(response.source, {
                                                   ByteRange{0, 4},    // 0.0 "This"
                                                   ByteRange{4, 7},    // 0.1 " is"
                                                   ByteRange{7, 9},    // 0.2 " a"
                                                   ByteRange{9, 18},   // 0.3 " sentence"
                                                   ByteRange{18, 20},  // 0.4 " <"
                                                   ByteRange{20, 24},  // 0.5 "with"
                                                   ByteRange{24, 25},  // 0.6 ">"
                                                   ByteRange{25, 31},  // 0.7 " named"
                                                   ByteRange{31, 33},  // 0.8 " &"
                                                   ByteRange{33, 42},  // 0.9 " entities"
                                                   ByteRange{42, 42}   // 0.10 ""
                                               });

  html.Restore(response);

  std::vector<std::string> html_tokens{"",          "<p data-attr=\"&quot;&apos;\">This",
                                       " is",       " a",
                                       " sentence",
                                       " &lt;",  // Oh trouble! The < is completely 'consumed'
                                       "with",      "&gt;",
                                       " named",    " &amp;",
                                       " entities", "",
                                       "</p>\n"};

  CHECK(AsTokens(response.source) == html_tokens);
}

TEST_CASE("Test self-closing tags should be treated as spaces") {
  std::string input("<p>Space<br>please?</p>\n");

  HTML html(std::move(input), true);
  CHECK(input == "Space please?\n");
}

TEST_CASE("Test reconstruction of target sentence") {
  std::string input("<p>hello <b>world</b></p>\n");
  HTML html(std::move(input), true);
  CHECK(input == "hello world\n");

  AnnotatedText source("hello world\n");
  RecordSentenceFromByteRange(source, {
                                          ByteRange{0, 4},   // 0.0 "hell"
                                          ByteRange{4, 5},   // 0.1 "o"
                                          ByteRange{5, 11},  // 0.2 " world"
                                          ByteRange{11, 11}  // 0.3 ""
                                      });

  AnnotatedText target("hallo Welt\n");
  RecordSentenceFromByteRange(target, {
                                          ByteRange{0, 4},   // 0.0 "hall"
                                          ByteRange{4, 5},   // 0.1 "o"
                                          ByteRange{5, 10},  // 0.2 " Welt"
                                          ByteRange{10, 10}  // 0.3 ""
                                      });

  Response response;
  response.source = source;
  response.target = target;

  html.Restore(response);

  std::vector<std::string> html_tokens_source{"", "<p>hell", "o", " <b>world", "", "</b></p>\n"};

  std::vector<std::string> html_tokens_target{"", "<p>hall", "o", " <b>Welt", "", "</b></p>\n"};

  CHECK(AsTokens(response.source) == html_tokens_source);
  CHECK(AsTokens(response.target) == html_tokens_target);
}

TEST_CASE("Test reconstruction of target sentence with entities") {
  std::string input("<p>hello <b>world &amp; friends!</b></p>\n");
  HTML html(std::move(input), true);
  CHECK(input == "hello world & friends!\n");

  AnnotatedText source("hello world & friends!\n");
  RecordSentenceFromByteRange(source, {
                                          ByteRange{0, 4},    // 0.0 "hell"
                                          ByteRange{4, 5},    // 0.1 "o"
                                          ByteRange{5, 11},   // 0.2 " world"
                                          ByteRange{11, 13},  // 0.3 " &"
                                          ByteRange{13, 21},  // 0.4 " friends"
                                          ByteRange{21, 22},  // 0.5 "!"
                                          ByteRange{22, 22}   // 0.6 ""
                                      });

  AnnotatedText target("hallo Welt & Freunde!\n");
  RecordSentenceFromByteRange(target, {
                                          ByteRange{0, 4},    // 0.0 "hall"
                                          ByteRange{4, 5},    // 0.1 "o"
                                          ByteRange{5, 10},   // 0.2 " Welt"
                                          ByteRange{10, 12},  // 0.3 " &"
                                          ByteRange{12, 20},  // 0.4 " Freunde"
                                          ByteRange{20, 21},  // 0.5 "!"
                                          ByteRange{21, 21}   // 0.6 ""
                                      });

  Response response;
  response.source = source;
  response.target = target;

  html.Restore(response);

  std::vector<std::string> html_tokens_source{"",         "<p>hell", "o", " <b>world", " &amp;",
                                              " friends", "!",       "",  "</b></p>\n"};

  std::vector<std::string> html_tokens_target{"",         "<p>hall", "o", " <b>Welt",  " &amp;",

                                              " Freunde", "!",       "",  "</b></p>\n"};

  CHECK(AsTokens(response.source) == html_tokens_source);
  CHECK(AsTokens(response.target) == html_tokens_target);
}

TEST_CASE("Test reconstruction of target with multiple sentences") {
  std::string input(
      "<p>hello <b>world!</b> How does this <img> <b>deal <u>with multiple sentences?</u></b> Will it work?</p>\n");
  HTML html(std::move(input), true);

  AnnotatedText source("hello world! How does this  deal with multiple sentences? Will it work?\n");
  CHECK(source.text == input);

  RecordSentenceFromByteRange(source, {
                                          ByteRange{0, 4},    // 0.0 "hell"
                                          ByteRange{4, 5},    // 0.1 "o"
                                          ByteRange{5, 11},   // 0.2 " world"
                                          ByteRange{11, 12},  // 0.3 "!"
                                          ByteRange{12, 12}   // 0.4 ""
                                      });
  RecordSentenceFromByteRange(source, {
                                          ByteRange{13, 16},  // 1.0 "How"
                                          ByteRange{16, 21},  // 1.1 " does"
                                          ByteRange{21, 26},  // 1.2 " this"
                                          ByteRange{26, 32},  // 1.3 "  deal"
                                          ByteRange{32, 37},  // 1.4 " with"
                                          ByteRange{37, 46},  // 1.5 " multiple"
                                          ByteRange{46, 55},  // 1.6 " sentence"
                                          ByteRange{55, 56},  // 1.7 "s"
                                          ByteRange{56, 57},  // 1.8 "?"
                                          ByteRange{57, 57}   // 1.9 ""
                                      });
  RecordSentenceFromByteRange(source, {
                                          ByteRange{58, 62},  // 2.0 "Will"
                                          ByteRange{62, 65},  // 2.1 " it"
                                          ByteRange{65, 70},  // 2.2 " work"
                                          ByteRange{70, 71},  // 2.3 "?"
                                          ByteRange{71, 71}   // 2.4 ""
                                      });

  AnnotatedText target("hallo Welt! Wie geht das mit mehreren Sätzen um? Wird es funktionieren?\n");
  RecordSentenceFromByteRange(target, {
                                          ByteRange{0, 4},    // 0.0 "hall"
                                          ByteRange{4, 5},    // 0.1 "o"
                                          ByteRange{5, 10},   // 0.2 " Welt"
                                          ByteRange{10, 11},  // 0.3 "!"
                                          ByteRange{11, 11},  // 0.4 ""
                                      });
  RecordSentenceFromByteRange(target, {
                                          ByteRange{12, 15},  // 1.0 "Wie"
                                          ByteRange{15, 20},  // 1.1 " geht"
                                          ByteRange{20, 24},  // 1.2 " das"
                                          ByteRange{24, 28},  // 1.3 " mit"
                                          ByteRange{28, 37},  // 1.4 " mehreren"
                                          ByteRange{37, 44},  // 1.5 " Sätze"
                                          ByteRange{44, 45},  // 1.6 "n"
                                          ByteRange{45, 48},  // 1.7 " um"
                                          ByteRange{48, 49},  // 1.8 "?"
                                          ByteRange{49, 49},  // 1.9 ""
                                      });
  RecordSentenceFromByteRange(target, {
                                          ByteRange{50, 54},  // 2.0 "Wird"
                                          ByteRange{54, 57},  // 2.1 " es"
                                          ByteRange{57, 71},  // 2.2 " funktionieren"
                                          ByteRange{71, 72},  // 2.3 "?"
                                          ByteRange{72, 72},  // 2.4 ""
                                      });

  std::vector<std::string> text_tokens_source{
      "",       "hall", "o",   " Welt", "!", "",  " ",    "Wie", " geht",          " das", " mit", " mehreren",
      " Sätze", "n",    " um", "?",     "",  " ", "Wird", " es", " funktionieren", "?",    "",     "\n"};

  CHECK(AsTokens(target) == text_tokens_source);

  Response response;
  response.source = source;
  response.target = target;
  html.Restore(response);

  std::vector<std::string> html_tokens_source{"",
                                              "<p>hell",
                                              "o",
                                              " <b>world",
                                              "!",
                                              "",
                                              "</b> ",
                                              "How",
                                              " does",
                                              " this",
                                              "  <img><b>deal",  // note how both spaces moved to __deal
                                              " <u>with",
                                              " multiple",
                                              " sentence",
                                              "s",
                                              "?",
                                              "",
                                              "</u></b> ",
                                              "Will",
                                              " it",
                                              " work",
                                              "?",
                                              "",
                                              "</p>\n"};
  CHECK(AsTokens(response.source) == html_tokens_source);
}

TEST_CASE("Test self-closing tag (HTML5)") {
  std::string input("<p>hello <img> <b>world</b> <u>and other <a href=\"#\">creatures</a></u></p>\n");
  HTML html(std::move(input), true);
}
