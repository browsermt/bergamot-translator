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
      ByteRange{0, 0 + 0},    // (start of sentence)
      ByteRange{0, 0 + 21},   // <p><input>H<u>e</u>ll
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
  std::vector<std::string> restored_tokens_str{"",
                                               "<p><input><u></u>Hell",  // Should really be "<p><input>H<u>e</u>ll"
                                               "o",
                                               " <b>world",
                                               "</b> how",
                                               " <u>are",
                                               " you",
                                               "</u>?",
                                               "",  // end of sentence
                                               "</p>\n"};

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
  CHECK(input == "hello  world and other creatures\n");  // Note double space between "hello" and "world"
}

TEST_CASE("Test empty self-closing tag at end of input") {
  std::string input("hello <br>");
  HTML html(std::move(input), true);
  CHECK(input == "hello ");
}

TEST_CASE("Test empty tag pair at end of input") {
  std::string input("hello <u></u>");
  HTML html(std::move(input), true);
  CHECK(input == "hello ");
}

TEST_CASE("Test empty self-closing pair at end of input in parent") {
  std::string input("<p>hello <br></p>");
  HTML html(std::move(input), true);
  CHECK(input == "hello ");
}


TEST_CASE("Test empty tag", "[!mayfail]") {
  std::string input(
      "<p id=\"1\">hello <img id=\"1.1\"><span id=\"1.2\"><u id=\"1.2.1\"></u><b id=\"1.2.2\"></b><img "
      "id=\"1.2.3\">world</span></p>\n");
  HTML html(std::move(input), true);
  CHECK(input == "hello world\n");

  Response response;

  std::string sentence_str("hello world");
  std::vector<string_view> sentence{
      string_view(sentence_str.data() + 0, 4),   // 0.0 hell
      string_view(sentence_str.data() + 4, 1),   // 0.1 o
      string_view(sentence_str.data() + 5, 6),   // 0.2 _world
      string_view(sentence_str.data() + 11, 0),  // 0.3 ""
  };
  response.source.appendSentence("", sentence.begin(), sentence.end());
  response.source.appendEndingWhitespace("\n");

  html.Restore(response);
  CHECK(response.source.text ==
        "<p id=\"1\">hello <img id=\"1.1\"><span id=\"1.2\"><u id=\"1.2.1\"></u><b id=\"1.2.2\"></b><img "
        "id=\"1.2.3\">world</span></p>\n");
}

TEST_CASE("End-to-end translation") {
  std::string input("<p>I <b>like</b> to <u>drive</u> this car.</p>\n");
  HTML html(std::move(input), true);
  CHECK(input == "I like to drive this car.\n");

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
    response.source.appendEndingWhitespace("\n");
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
    response.target.appendEndingWhitespace("\n");
  }

  html.Restore(response);

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
    source.appendEndingWhitespace("</p>\n");

    CHECK(AsTokens(response.source) == AsTokens(source));
  }

  {
    AnnotatedText target;
    std::string sentence_str("<p>Ich <u>fahre</u> <b>gerne</b> dieses Auto.");
    std::vector<string_view> sentence{
        string_view(sentence_str.data() + 0, 6),    // 0.0 "<p>Ich"
        string_view(sentence_str.data() + 6, 4),    // 0.1 " <u>"
        string_view(sentence_str.data() + 10, 4),   // 0.2 "fahr"
        string_view(sentence_str.data() + 14, 1),   // 0.3 "e"
        string_view(sentence_str.data() + 15, 13),  // 0.4 "</u> <b>gerne"
        string_view(sentence_str.data() + 28, 11),  // 0.5 "</b> dieses"
        string_view(sentence_str.data() + 39, 5),   // 0.6 " Auto"
        string_view(sentence_str.data() + 44, 1),   // 0.7 "."
        string_view(sentence_str.data() + 45, 0),   // 0.8 ""
    };
    target.appendSentence("", sentence.begin(), sentence.end());
    target.appendEndingWhitespace("</p>\n");

    CHECK(AsTokens(response.target) == AsTokens(target));
  }
}

// TEST_CASE("")