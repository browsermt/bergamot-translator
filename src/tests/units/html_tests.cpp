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

std::vector<ByteRange> asByteRanges(AnnotatedText const &annotation) {
  std::vector<ByteRange> words;
  words.emplace_back(annotation.annotation.gap(0));
  for (size_t sentenceIdx = 0; sentenceIdx < annotation.numSentences(); ++sentenceIdx) {
    for (size_t wordIdx = 0; wordIdx < annotation.numWords(sentenceIdx); ++wordIdx)
      words.emplace_back(annotation.wordAsByteRange(sentenceIdx, wordIdx));
    words.emplace_back(annotation.annotation.gap(sentenceIdx + 1));
  }
  return words;
}

std::vector<std::string> asTokens(AnnotatedText const &annotation) {
  std::vector<std::string> words;
  words.emplace_back(annotation.gap(0));
  for (size_t sentenceIdx = 0; sentenceIdx < annotation.numSentences(); ++sentenceIdx) {
    for (size_t wordIdx = 0; wordIdx < annotation.numWords(sentenceIdx); ++wordIdx)
      words.emplace_back(annotation.word(sentenceIdx, wordIdx));
    words.emplace_back(annotation.gap(sentenceIdx + 1));
  }
  return words;
}

void recordSentenceFromByteRange(AnnotatedText &text, std::vector<ByteRange> const &ranges) {
  assert(ranges.size() > 0);

  std::vector<string_view> tokens;
  tokens.reserve(ranges.size());

  for (auto &&range : ranges) tokens.emplace_back(text.text.data() + range.begin, range.size());

  text.recordExistingSentence(tokens.begin(), tokens.end(), text.text.data() + ranges[0].begin);
}

template <typename T>
std::vector<std::vector<T>> identity_matrix(size_t size) {
  std::vector<std::vector<T>> rows(size);
  for (size_t row = 0; row < size; ++row) {
    rows[row].resize(size, T(0));
    rows[row][row] = T(1);
  }
  return rows;
}

TEST_CASE("Ignore HTML if process_markup is false") {
  std::string html_code("<p>This text &amp; has <b>HTML</b> in it</p>");

  std::string input(html_code);
  HTML html(std::move(input), false);
  CHECK(input == html_code);

  Response response;
  response.source.text = html_code;
  response.target.text = html_code;
  // Note: response.alignments is empty, which is allowed in this case
  html.restore(response);

  // Assert that restore() does not mess with my HTML code
  CHECK(response.source.text == html_code);
}

TEST_CASE("Abort if alignments are missing") {
  marian::setThrowExceptionOnAbort(true);

  std::string input("<p>hello <b>world</b></p>\n");
  HTML html(std::move(input), true);

  AnnotatedText source("hello world\n");
  recordSentenceFromByteRange(source, {
                                          ByteRange{0, 4},   // 0.0 "hell"
                                          ByteRange{4, 5},   // 0.1 "o"
                                          ByteRange{5, 11},  // 0.2 " world"
                                          ByteRange{11, 11}  // 0.3 ""
                                      });

  AnnotatedText target("hallo Welt\n");
  recordSentenceFromByteRange(target, {
                                          ByteRange{0, 4},   // 0.0 "hall"
                                          ByteRange{4, 5},   // 0.1 "o"
                                          ByteRange{5, 10},  // 0.2 " Welt"
                                          ByteRange{10, 10}  // 0.3 ""
                                      });

  Response response;
  response.source = source;
  response.target = target;
  // Note: explicitly not setting response.alignments

  CHECK_THROWS_WITH(
      html.restore(response),
      "Response object does not contain alignments. TranslationModel or ResponseOptions is misconfigured?");
}

TEST_CASE("Abort if alignments are misconfigured") {
  marian::setThrowExceptionOnAbort(true);

  std::string input("<p>hello <b>world</b></p>\n");
  HTML html(std::move(input), true);

  AnnotatedText source("hello world\n");
  recordSentenceFromByteRange(source, {
                                          ByteRange{0, 4},   // 0.0 "hell"
                                          ByteRange{4, 5},   // 0.1 "o"
                                          ByteRange{5, 11},  // 0.2 " world"
                                          ByteRange{11, 11}  // 0.3 ""
                                      });

  AnnotatedText target("hallo Welt\n");
  recordSentenceFromByteRange(target, {
                                          ByteRange{0, 4},   // 0.0 "hall"
                                          ByteRange{4, 5},   // 0.1 "o"
                                          ByteRange{5, 10},  // 0.2 " Welt"
                                          ByteRange{10, 10}  // 0.3 ""
                                      });

  Response response;
  response.source = source;
  response.target = target;

  // If the model is misconfigured to not give any alignment information,
  // response will have entries for each target word, but they will all be empty.
  response.alignments = {{{}, {}, {}, {}}};

  CHECK_THROWS_WITH(
      html.restore(response),
      "Response object does not contain alignments. TranslationModel or ResponseOptions is misconfigured?");
}

TEST_CASE("Do not abort if the input is just empty") {
  std::string input("");
  HTML html(std::move(input), true);
  CHECK(input == "");

  Response response;
  html.restore(response);
  CHECK(response.source.text == "");
  CHECK(response.target.text == "");
}

TEST_CASE("Do not abort if the input is just empty element") {
  std::string input("<p></p>");
  HTML html(std::move(input), true);
  CHECK(input == "");

  Response response;
  html.restore(response);
  CHECK(response.source.text == "<p></p>");
  CHECK(response.target.text == "<p></p>");
}

TEST_CASE("Test case html entities") {
  // These are all entities I would expect in innerHTML, since all other entities
  // can be encoded as UTF-8 so there's no need to encode them through &...; when
  // innerHTML encodes the DOM as HTML.
  std::string input("<p data-attr=\"&quot;&apos;\">This is a sentence &lt;with&gt; named &amp; entities</p>");
  HTML html(std::move(input), true);
  CHECK(input == "This is a sentence <with> named & entities");
}

TEST_CASE("Test self-closing tags should be treated as paragraph break") {
  std::string test_str("<p>Space<br>please?</p>");

  std::string input(test_str);
  HTML html(std::move(input), true);
  CHECK(input == "Space\n\nplease?");

  Response response;
  std::string source_str("Space\n\nplease?");
  std::vector<string_view> source_tokens{
      string_view(source_str.data() + 0, 5),   // Space
      string_view(source_str.data() + 5, 0),   // [EOS]
      string_view(source_str.data() + 5, 2),   // \n\n
      string_view(source_str.data() + 7, 1),   // p
      string_view(source_str.data() + 8, 5),   // lease
      string_view(source_str.data() + 13, 1),  // ?
      string_view(source_str.data() + 14, 0),  // EOS
  };
  response.source.appendSentence("", source_tokens.begin(), source_tokens.begin() + 2);
  response.source.appendSentence("\n\n", source_tokens.begin() + 3, source_tokens.end());

  std::string target_str("Platz\n\nbitte?");
  std::vector<string_view> target_tokens{
      string_view(target_str.data() + 0, 5),   // Platz
      string_view(target_str.data() + 5, 0),   // [EOS]
      string_view(target_str.data() + 5, 2),   // \n\n
      string_view(target_str.data() + 7, 5),   // bitte
      string_view(target_str.data() + 12, 1),  // ?
      string_view(target_str.data() + 13, 0),  // [EOS]
  };
  response.target.appendSentence("", target_tokens.begin(), target_tokens.begin() + 2);
  response.target.appendSentence("", target_tokens.begin() + 3, target_tokens.end());
  response.alignments = {{
                             {1.0, 0.0},  //  Platz <- Space
                             {0.0, 1.0}   //  [EOS] <- [EOS]
                         },
                         {
                             {0.1, 0.9, 0.0, 0.0},  // _bitte <- _p + lease
                             {0.0, 0.0, 1.0, 0.0},  //      ? <- ?
                             {0.0, 0.0, 0.0, 1.0},  //  [EOS] <- [EOS]
                         }};

  // Main focus of this test is that the space that was introduced in the text
  // that was being translated does not end up in the translation.
  html.restore(response);
  CHECK(response.source.text == "<p>Space<br>please?</p>");
  CHECK(response.target.text == "<p>Platz<br>bitte?</p>");
}

TEST_CASE("Test inline tags should be treated as spaces") {
  std::string test_str("un<u>der</u>line");

  std::string input(test_str);
  HTML html(std::move(input), true);
  CHECK(input == "un der line");

  Response response;
  std::string source_str("un der line");
  std::vector<string_view> source_tokens{
      string_view(source_str.data() + 0, 2),   // un
      string_view(source_str.data() + 2, 3),   // _de
      string_view(source_str.data() + 5, 1),   // r
      string_view(source_str.data() + 6, 5),   // _line
      string_view(source_str.data() + 11, 0),  // EOS
  };
  response.source.appendSentence("", source_tokens.begin(), source_tokens.end());

  std::string target_str("una linea der");
  std::vector<string_view> target_tokens{
      string_view(target_str.data() + 0, 3),   // una
      string_view(target_str.data() + 3, 6),   // _linéa
      string_view(target_str.data() + 9, 3),   // _de
      string_view(target_str.data() + 12, 1),  // r
      string_view(target_str.data() + 13, 0),  // [EOS]
  };
  response.target.appendSentence("", target_tokens.begin(), target_tokens.end());

  response.alignments = {{{0.9795, 0.0127, 0.0002, 0.0066, 0.0009},
                          {0.0098, 0.2967, 0.0156, 0.6640, 0.0138},
                          {0.0214, 0.7472, 0.0626, 0.0745, 0.0943},
                          {0.0022, 0.0230, 0.9357, 0.0165, 0.0226},
                          {0.0122, 0.0240, 0.0085, 0.7427, 0.2125}}};

  html.restore(response);
  CHECK(response.source.text == "un <u>der</u> line");  // TODO leave spaces?
  CHECK(response.target.text == "una linea <u>der</u>");
}

TEST_CASE("Test inline tags should not break words") {
  std::string test_str("un<u>der</u>line");

  std::string input(test_str);
  HTML::Options options;
  options.substituteInlineTagsWithSpaces = false;
  HTML html(std::move(input), true, std::move(options));
  CHECK(input == "underline");

  Response response;
  std::string source_str("underline");
  std::vector<string_view> source_tokens{
      string_view(source_str.data() + 0, 9),  // underline
      string_view(source_str.data() + 9, 0),  // EOS
  };
  response.source.appendSentence("", source_tokens.begin(), source_tokens.end());

  std::string target_str("subrayar");
  std::vector<string_view> target_tokens{
      string_view(target_str.data() + 0, 8),  // subrayar
      string_view(target_str.data() + 8, 0),  // [EOS]
  };
  response.target.appendSentence("", target_tokens.begin(), target_tokens.end());

  response.alignments = {identity_matrix<float>(2)};

  html.restore(response);
  CHECK(response.source.text == "<u></u>underline");  // TODO not spread <u> to whole word?
  CHECK(response.target.text == "<u></u>subrayar");   // TODO not spread <u> to the whole word?
}

TEST_CASE("Test reconstruction of target sentence") {
  std::string input("<p>hello <b>world</b></p>\n");
  HTML html(std::move(input), true);
  CHECK(input == "hello world\n\n\n");  // tripple \n because \n + </p>

  AnnotatedText source("hello world\n\n\n");
  recordSentenceFromByteRange(source, {
                                          ByteRange{0, 4},   // 0.0 "hell"
                                          ByteRange{4, 5},   // 0.1 "o"
                                          ByteRange{5, 11},  // 0.2 " world"
                                          ByteRange{11, 11}  // 0.3 ""
                                      });

  AnnotatedText target("hallo Welt\n\n\n");
  recordSentenceFromByteRange(target, {
                                          ByteRange{0, 4},   // 0.0 "hall"
                                          ByteRange{4, 5},   // 0.1 "o"
                                          ByteRange{5, 10},  // 0.2 " Welt"
                                          ByteRange{10, 10}  // 0.3 ""
                                      });

  Response response;
  response.source = source;
  response.target = target;
  response.alignments = {identity_matrix<float>(4)};

  html.restore(response);

  std::vector<std::string> html_tokens_source{"", "<p>hell", "o", " <b>world", "", "</b></p>\n"};

  std::vector<std::string> html_tokens_target{"", "<p>hall", "o", " <b>Welt", "", "</b></p>\n"};

  CHECK(asTokens(response.source) == html_tokens_source);
  CHECK(asTokens(response.target) == html_tokens_target);
}

TEST_CASE("Test reconstruction of target sentence with entities") {
  std::string input("<p>hello <b>world &amp; friends!</b></p>");
  HTML html(std::move(input), true);
  CHECK(input == "hello world & friends!");

  AnnotatedText source("hello world & friends!");
  recordSentenceFromByteRange(source, {
                                          ByteRange{0, 4},    // 0.0 "hell"
                                          ByteRange{4, 5},    // 0.1 "o"
                                          ByteRange{5, 11},   // 0.2 " world"
                                          ByteRange{11, 13},  // 0.3 " &"
                                          ByteRange{13, 21},  // 0.4 " friends"
                                          ByteRange{21, 22},  // 0.5 "!"
                                          ByteRange{22, 22}   // 0.6 ""
                                      });

  AnnotatedText target("hallo Welt & Freunde!");
  recordSentenceFromByteRange(target, {
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
  response.alignments = {identity_matrix<float>(7)};

  html.restore(response);

  std::vector<std::string> html_tokens_source{"",         "<p>hell", "o", " <b>world", " &amp;",
                                              " friends", "!",       "",  "</b></p>"};

  std::vector<std::string> html_tokens_target{"",         "<p>hall", "o", " <b>Welt", " &amp;",

                                              " Freunde", "!",       "",  "</b></p>"};

  CHECK(asTokens(response.source) == html_tokens_source);
  CHECK(asTokens(response.target) == html_tokens_target);
}

TEST_CASE("Test reconstruction of target with multiple sentences") {
  std::string input(
      "<p>hello <b>world!</b> How does this <img> <b>deal <u>with multiple sentences?</u></b> Will it work?</p>");
  HTML html(std::move(input), true);

  AnnotatedText source("hello world! How does this  deal with multiple sentences? Will it work?");
  CHECK(source.text == input);

  recordSentenceFromByteRange(source, {
                                          ByteRange{0, 4},    // 0.0 "hell"
                                          ByteRange{4, 5},    // 0.1 "o"
                                          ByteRange{5, 11},   // 0.2 " world"
                                          ByteRange{11, 12},  // 0.3 "!"
                                          ByteRange{12, 12}   // 0.4 ""
                                      });
  recordSentenceFromByteRange(source, {
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
  recordSentenceFromByteRange(source, {
                                          ByteRange{58, 62},  // 2.0 "Will"
                                          ByteRange{62, 65},  // 2.1 " it"
                                          ByteRange{65, 70},  // 2.2 " work"
                                          ByteRange{70, 71},  // 2.3 "?"
                                          ByteRange{71, 71}   // 2.4 ""
                                      });

  AnnotatedText target("hallo Welt! Wie geht das mit mehreren Sätzen um? Wird es funktionieren?");
  recordSentenceFromByteRange(target, {
                                          ByteRange{0, 4},    // 0.0 "hall"
                                          ByteRange{4, 5},    // 0.1 "o"
                                          ByteRange{5, 10},   // 0.2 " Welt"
                                          ByteRange{10, 11},  // 0.3 "!"
                                          ByteRange{11, 11},  // 0.4 ""
                                      });
  recordSentenceFromByteRange(target, {
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
  recordSentenceFromByteRange(target, {
                                          ByteRange{50, 54},  // 2.0 "Wird"
                                          ByteRange{54, 57},  // 2.1 " es"
                                          ByteRange{57, 71},  // 2.2 " funktionieren"
                                          ByteRange{71, 72},  // 2.3 "?"
                                          ByteRange{72, 72},  // 2.4 ""
                                      });

  std::vector<std::string> text_tokens_source{
      "",       "hall", "o",   " Welt", "!", "",  " ",    "Wie", " geht",          " das", " mit", " mehreren",
      " Sätze", "n",    " um", "?",     "",  " ", "Wird", " es", " funktionieren", "?",    "",     ""};

  CHECK(asTokens(target) == text_tokens_source);

  Response response;
  response.source = source;
  response.target = target;
  response.alignments = {identity_matrix<float>(5), identity_matrix<float>(10), identity_matrix<float>(5)};
  html.restore(response);

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
                                              "</p>"};
  CHECK(asTokens(response.source) == html_tokens_source);
}

TEST_CASE("Test self-closing tag (HTML5)") {
  std::string input("<p>hello <img> <b>world</b> <u>and other <a href=\"#\">creatures</a></u></p>");
  HTML html(std::move(input), true);
  CHECK(input == "hello  world and other creatures");  // Note double space between "hello" and "world"
}

TEST_CASE("Test empty void tag at end of input") {
  std::string input("hello <br>");
  HTML html(std::move(input), true);
  CHECK(input == "hello ");

  Response response;
  std::string sentence_str("hello ");
  std::vector<string_view> sentence{
      string_view(sentence_str.data() + 0, 4),  // 0.0 hell
      string_view(sentence_str.data() + 4, 2),  // 0.1 o_
      string_view(sentence_str.data() + 6, 0),  // 0.2 [EOS]
  };
  response.source.appendSentence("", sentence.begin(), sentence.end());
  response.target.appendSentence("", sentence.begin(), sentence.end());
  response.alignments = {identity_matrix<float>(3)};

  html.restore(response);
  CHECK(response.source.text == "hello <br>");
  CHECK(response.target.text == "hello <br>");
}

TEST_CASE("Test empty tag pair at end of input") {
  std::string input("hello <u></u>");
  HTML html(std::move(input), true);
  CHECK(input == "hello ");

  Response response;
  std::string sentence_str("hello ");
  std::vector<string_view> sentence{
      string_view(sentence_str.data() + 0, 4),  // 0.0 hell
      string_view(sentence_str.data() + 4, 2),  // 0.1 o_
      string_view(sentence_str.data() + 6, 0),  // 0.2 [EOS]
  };
  response.source.appendSentence("", sentence.begin(), sentence.end());
  response.target.appendSentence("", sentence.begin(), sentence.end());
  response.alignments = {identity_matrix<float>(3)};

  html.restore(response);
  CHECK(response.source.text == "hello <u></u>");
  CHECK(response.target.text == "hello <u></u>");
}

TEST_CASE("Test empty self-closing pair at end of input in parent") {
  std::string input("<p>hello <br></p>");
  HTML html(std::move(input), true);
  CHECK(input == "hello ");
}

TEST_CASE("Test empty tag") {
  std::string test_str(
      "<p id=\"1\">hello <img id=\"1.1\"><span id=\"1.2\"><u id=\"1.2.1\"></u><b id=\"1.2.2\"></b><img "
      "id=\"1.2.3\">world</span></p>");

  std::string input(test_str);
  HTML html(std::move(input), true);
  CHECK(input == "hello world");

  Response response;

  std::string sentence_str("hello world");
  std::vector<string_view> sentence{
      string_view(sentence_str.data() + 0, 4),   // 0.0 hell
      string_view(sentence_str.data() + 4, 1),   // 0.1 o
      string_view(sentence_str.data() + 5, 6),   // 0.2 _world
      string_view(sentence_str.data() + 11, 0),  // 0.3 ""
  };
  response.source.appendSentence("", sentence.begin(), sentence.end());
  response.target.appendSentence("", sentence.begin(), sentence.end());
  response.alignments = {identity_matrix<float>(4)};

  html.restore(response);
  CHECK(response.source.text == test_str);
  CHECK(response.target.text == test_str);
}

TEST_CASE("Test <script> element") {
  std::string test_str("hello <script>alert(\"<foo>\");</script>world");

  std::string input(test_str);
  HTML html(std::move(input), true);
  CHECK(input == "hello \n\nworld");

  Response response;
  std::string sentence_str("hello \n\nworld");
  std::vector<string_view> sentence{
      string_view(sentence_str.data() + 0, 4),   // 0.0 hell
      string_view(sentence_str.data() + 4, 2),   // 0.1 o_
      string_view(sentence_str.data() + 6, 2),   // 0.2 \n\n
      string_view(sentence_str.data() + 8, 5),   // 0.3 world
      string_view(sentence_str.data() + 13, 0),  // 0.4 ""
  };
  response.source.appendSentence("", sentence.begin(), sentence.end());
  response.target.appendSentence("", sentence.begin(), sentence.end());
  response.alignments = {identity_matrix<float>(5)};

  html.restore(response);
  CHECK(response.source.text == test_str);
  CHECK(response.target.text == test_str);
}

TEST_CASE("Test comment") {
  std::string test_str("foo <!-- <ignore> me -->bar");

  std::string input(test_str);
  HTML html(std::move(input), true);
  CHECK(input == "foo bar");

  Response response;
  std::string sentence_str("foo bar");
  std::vector<string_view> sentence{
      string_view(sentence_str.data() + 0, 3),  // foo
      string_view(sentence_str.data() + 3, 4),  // _bar
      string_view(sentence_str.data() + 7, 0),  // ""
  };
  response.source.appendSentence("", sentence.begin(), sentence.end());
  response.target.appendSentence("", sentence.begin(), sentence.end());
  response.alignments = {identity_matrix<float>(3)};

  html.restore(response);
  CHECK(response.source.text == test_str);
  CHECK(response.target.text == test_str);
}

TEST_CASE("End-to-end translation", "[!mayfail]") {
  std::string input("<p>I <b>like</b> to <u>drive</u> this car.</p>");
  HTML html(std::move(input), true);
  CHECK(input == "I like to drive this car.");

  Response response;

  // clang-format off
  response.alignments = std::vector<std::vector<std::vector<float>>>{{
    {0.982376,  0.00742467, 0.00682965, 0.00121767, 0.000848056,6.51436e-05,7.53791e-06,0.00123162},
    {0.165639,  0.368694,   0.230394,   0.222476,   0.00349563, 0.00105052, 0.000603092,0.00764845},
    {0.00493271,0.0805876,  0.0139988,  0.89116,    0.000928116,0.00200724, 0.000512013,0.00587302},
    {0.0194648, 0.411029,   0.087059,   0.0477847,  0.26596,    0.111161,   0.000392092,0.0571499},
    {0.00879706,0.492504,   0.0448291,  0.007779,   0.423114,   0.0125523,  0.00119587, 0.00922804},
    {0.00181909,0.00603626, 0.0335758,  0.037193,   0.747266,   0.102497,   0.0585782,  0.0130341},
    {4.1348e-06,0.000156165,2.16369e-05,0.00275059, 0.00183456, 0.992357,   0.0023765,  0.000499018},
    {0.00149043,0.000719392,0.0168534,  0.00430164, 0.00200343, 0.0106381,  0.948566,   0.0154279},
    {0.0903136, 0.0550843,  0.0699474,  0.0792285,  0.223006,   0.207565,   0.129241,   0.145614},
  }};
  // clang-format on

  {
    std::string sentence_str("I like to drive this car.");
    std::vector<string_view> sentence{
        string_view(sentence_str.data() + 0, 1),   // 0.0 "I"
        string_view(sentence_str.data() + 1, 5),   // 0.1 " like"
        string_view(sentence_str.data() + 6, 3),   // 0.2 " to"
        string_view(sentence_str.data() + 9, 6),   // 0.3 " drive"
        string_view(sentence_str.data() + 15, 5),  // 0.4 " this"
        string_view(sentence_str.data() + 20, 4),  // 0.5 " car"
        string_view(sentence_str.data() + 24, 1),  // 0.6 "."
        string_view(sentence_str.data() + 25, 0),  // 0.7 ""
    };
    response.source.appendSentence("", sentence.begin(), sentence.end());
  }

  {
    std::string sentence_str("Ich fahre gerne dieses Auto.");
    std::vector<string_view> sentence{
        string_view(sentence_str.data() + 0, 3),   // 0.0 "Ich"
        string_view(sentence_str.data() + 3, 1),   // 0.1 " "
        string_view(sentence_str.data() + 4, 4),   // 0.2 "fahr"
        string_view(sentence_str.data() + 8, 1),   // 0.3 "e"
        string_view(sentence_str.data() + 9, 6),   // 0.4 " gerne"
        string_view(sentence_str.data() + 15, 7),  // 0.5 " dieses"
        string_view(sentence_str.data() + 22, 5),  // 0.6 " Auto"
        string_view(sentence_str.data() + 27, 1),  // 0.7 "."
        string_view(sentence_str.data() + 28, 0),  // 0.8 ""
    };
    response.target.appendSentence("", sentence.begin(), sentence.end());
  }

  html.restore(response);

  {
    AnnotatedText source;
    std::string sentence_str("<p>I <b>like</b> to <u>drive</u> this car.");
    std::vector<string_view> sentence{
        string_view(sentence_str.data() + 0, 4),   // 0.0 "<p>I"
        string_view(sentence_str.data() + 4, 8),   // 0.1 " <b>like"
        string_view(sentence_str.data() + 12, 7),  // 0.2 "</b> to"
        string_view(sentence_str.data() + 19, 9),  // 0.3 " <u>drive"
        string_view(sentence_str.data() + 28, 9),  // 0.4 "</u> this"
        string_view(sentence_str.data() + 37, 4),  // 0.5 " car"
        string_view(sentence_str.data() + 41, 1),  // 0.6 "."
        string_view(sentence_str.data() + 42, 0),  // 0.7 ""
    };
    source.appendSentence("", sentence.begin(), sentence.end());
    source.appendEndingWhitespace("</p>");

    CHECK(asTokens(response.source) == asTokens(source));
  }

  {
    AnnotatedText target;
    // Empty <b></b> because the space token after "Ich" has "<p><b>" markup, passed down from "<b>like</b>"
    std::string sentence_str("<p>Ich <b></b><u>fahre</u> <b>gerne</b> dieses Auto.");
    std::vector<string_view> sentence{
        string_view(sentence_str.data() + 0, 6),    // 0.0 "<p>Ich"
        string_view(sentence_str.data() + 6, 4),    // 0.1 " <b>"
        string_view(sentence_str.data() + 10, 11),  // 0.2 "</b><u>fahr"
        string_view(sentence_str.data() + 21, 1),   // 0.3 "e"
        string_view(sentence_str.data() + 22, 13),  // 0.4 "</u> <b>gerne"
        string_view(sentence_str.data() + 35, 11),  // 0.5 "</b> dieses"
        string_view(sentence_str.data() + 46, 5),   // 0.6 " Auto"
        string_view(sentence_str.data() + 51, 1),   // 0.7 "."
        string_view(sentence_str.data() + 52, 0),   // 0.8 ""
    };
    target.appendSentence("", sentence.begin(), sentence.end());
    target.appendEndingWhitespace("</p>");

    CHECK(asTokens(response.target) == asTokens(target));
  }
}

TEST_CASE("End-to-end translation when no words with markup align", "[!mayfail]") {
  std::string input("<p>I <b>like</b> to <u>drive</u> this car.</p>");
  HTML html(std::move(input), true);
  CHECK(input == "I like to drive this car.");

  Response response;

  // clang-format off
  response.alignments = std::vector<std::vector<std::vector<float>>>{{
    {0.5360, 0.4405, 0.0142, 0.0061, 0.0029, 0.0001, 0.0000, 0.0001},
    {0.0451, 0.0602, 0.5120, 0.2584, 0.1145, 0.0062, 0.0019, 0.0017},
    {0.0392, 0.0009, 0.6535, 0.2293, 0.0492, 0.0199, 0.0014, 0.0067},
    {0.0007, 0.0036, 0.0112, 0.0118, 0.9209, 0.0449, 0.0050, 0.0019},
    {0.0000, 0.0004, 0.0008, 0.0047, 0.0163, 0.9683, 0.0045, 0.0050},
    {0.0011, 0.0046, 0.0039, 0.0090, 0.0023, 0.0024, 0.9648, 0.0119},
    {0.0840, 0.0744, 0.1545, 0.1330, 0.1818, 0.1722, 0.0859, 0.1143},
  }};
  // clang-format on

  {
    std::string sentence_str("I like to drive this car.");
    std::vector<string_view> sentence{
        string_view(sentence_str.data() + 0, 1),   // 0.0 "I"
        string_view(sentence_str.data() + 1, 5),   // 0.1 " like"
        string_view(sentence_str.data() + 6, 3),   // 0.2 " to"
        string_view(sentence_str.data() + 9, 6),   // 0.3 " drive"
        string_view(sentence_str.data() + 15, 5),  // 0.4 " this"
        string_view(sentence_str.data() + 20, 4),  // 0.5 " car"
        string_view(sentence_str.data() + 24, 1),  // 0.6 "."
        string_view(sentence_str.data() + 25, 0),  // 0.7 [EOS]
    };
    response.source.appendSentence("", sentence.begin(), sentence.end());
  }

  {
    std::string sentence_str("Rád řídím to auto.");
    std::vector<string_view> sentence{
        string_view(sentence_str.data() + 0, 4),   // 0.0 "Rád"
        string_view(sentence_str.data() + 4, 6),   // 0.1 " říd"
        string_view(sentence_str.data() + 10, 3),  // 0.2 "ím"
        string_view(sentence_str.data() + 13, 3),  // 0.3 "_to"
        string_view(sentence_str.data() + 16, 5),  // 0.4 " auto"
        string_view(sentence_str.data() + 21, 1),  // 0.5 "."
        string_view(sentence_str.data() + 22, 0),  // 0.6 [EOS]
    };
    response.target.appendSentence("", sentence.begin(), sentence.end());
  }

  html.restore(response);

  {
    AnnotatedText source;
    std::string sentence_str("<p>I <b>like</b> to <u>drive</u> this car.");
    std::vector<string_view> sentence{
        string_view(sentence_str.data() + 0, 4),   // 0.0 "<p>I"
        string_view(sentence_str.data() + 4, 8),   // 0.1 " <b>like"
        string_view(sentence_str.data() + 12, 7),  // 0.2 "</b> to"
        string_view(sentence_str.data() + 19, 9),  // 0.3 " <u>drive"
        string_view(sentence_str.data() + 28, 9),  // 0.4 "</u> this"
        string_view(sentence_str.data() + 37, 4),  // 0.5 " car"
        string_view(sentence_str.data() + 41, 1),  // 0.6 "."
        string_view(sentence_str.data() + 42, 0),  // 0.7 ""
    };
    source.appendSentence("", sentence.begin(), sentence.end());
    source.appendEndingWhitespace("</p>");

    CHECK(asTokens(response.source) == asTokens(source));
  }

  {
    AnnotatedText target;
    std::string sentence_str("<p>Rád <b></b>řídím <u></u>to auto.");
    std::vector<string_view> sentence{
        string_view(sentence_str.data() + 0, 7),    // 0.0 "<p>Rád"
        string_view(sentence_str.data() + 7, 13),   // 0.1 " <b></b>říd"
        string_view(sentence_str.data() + 20, 3),   // 0.2 "ím"
        string_view(sentence_str.data() + 23, 10),  // 0.3 "_<u></u>to"
        string_view(sentence_str.data() + 33, 5),   // 0.4 " auto"
        string_view(sentence_str.data() + 38, 1),   // 0.5 "."
        string_view(sentence_str.data() + 39, 0),   // 0.6 [EOS]
    };
    target.appendSentence("", sentence.begin(), sentence.end());
    target.appendEndingWhitespace("</p>");

    CHECK(asTokens(response.target) == asTokens(target));
  }
}

// TEST_CASE("")