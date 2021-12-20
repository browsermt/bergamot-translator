#include <string>

#include "catch.hpp"
#include "translator/xh_scanner.h"

TEST_CASE("scan element with attributes") {
  markup::instream in("<div id=\"test\" class=\"a b c \">");
  markup::Scanner scanner(in);

  CHECK(scanner.next() == markup::Scanner::TT_TAG_START);
  CHECK(scanner.tag() == "div");

  CHECK(scanner.next() == markup::Scanner::TT_ATTRIBUTE);
  CHECK(scanner.attribute() == "id");
  CHECK(scanner.value() == "test");

  CHECK(scanner.next() == markup::Scanner::TT_ATTRIBUTE);
  CHECK(scanner.attribute() == "class");
  CHECK(scanner.value() == "a b c ");

  CHECK(scanner.next() == markup::Scanner::TT_EOF);
}

TEST_CASE("scan element with valueless attributes") {
  markup::instream in("<input checked hidden>");
  markup::Scanner scanner(in);

  CHECK(scanner.next() == markup::Scanner::TT_TAG_START);
  CHECK(scanner.tag() == "input");

  CHECK(scanner.next() == markup::Scanner::TT_ATTRIBUTE);
  CHECK(scanner.attribute() == "checked");
  CHECK(scanner.value() == "");

  CHECK(scanner.next() == markup::Scanner::TT_ATTRIBUTE);
  CHECK(scanner.attribute() == "hidden");
  CHECK(scanner.value() == "");

  CHECK(scanner.next() == markup::Scanner::TT_EOF);
}

TEST_CASE("scan element with unquoted attributes") {
  markup::instream in("<div hidden=true class=test>");
  markup::Scanner scanner(in);

  CHECK(scanner.next() == markup::Scanner::TT_TAG_START);
  CHECK(scanner.tag() == "div");

  CHECK(scanner.next() == markup::Scanner::TT_ATTRIBUTE);
  CHECK(scanner.attribute() == "hidden");
  CHECK(scanner.value() == "true");

  CHECK(scanner.next() == markup::Scanner::TT_ATTRIBUTE);
  CHECK(scanner.attribute() == "class");
  CHECK(scanner.value() == "test");

  CHECK(scanner.next() == markup::Scanner::TT_EOF);
}

TEST_CASE("scan element with spaces around attributes") {
  markup::instream in("<input class = \"test\" checked type = checkbox >");
  markup::Scanner scanner(in);

  CHECK(scanner.next() == markup::Scanner::TT_TAG_START);
  CHECK(scanner.tag() == "input");

  CHECK(scanner.next() == markup::Scanner::TT_ATTRIBUTE);
  CHECK(scanner.attribute() == "class");
  CHECK(scanner.value() == "test");

  CHECK(scanner.next() == markup::Scanner::TT_ATTRIBUTE);
  CHECK(scanner.attribute() == "checked");
  CHECK(scanner.value() == "");

  CHECK(scanner.next() == markup::Scanner::TT_ATTRIBUTE);
  CHECK(scanner.attribute() == "type");
  CHECK(scanner.value() == "checkbox");

  CHECK(scanner.next() == markup::Scanner::TT_EOF);
}

TEST_CASE("scan element with text") {
  markup::instream in("<span>Hello world</span>");
  markup::Scanner scanner(in);

  CHECK(scanner.next() == markup::Scanner::TT_TAG_START);
  CHECK(scanner.next() == markup::Scanner::TT_TEXT);
  CHECK(scanner.value() == "Hello world");
  CHECK(scanner.next() == markup::Scanner::TT_TAG_END);
  CHECK(scanner.next() == markup::Scanner::TT_EOF);
}

TEST_CASE("scan html entities") {
  markup::instream in("&amp;&apos;&nbsp;&quot;&lt;&gt;");
  markup::Scanner scanner(in);

  CHECK(scanner.next() == markup::Scanner::TT_TEXT);
  CHECK(scanner.value() == "&");
  CHECK(scanner.next() == markup::Scanner::TT_TEXT);
  CHECK(scanner.value() == "'");
  CHECK(scanner.next() == markup::Scanner::TT_TEXT);
  CHECK(scanner.value() == " ");
  CHECK(scanner.next() == markup::Scanner::TT_TEXT);
  CHECK(scanner.value() == "\"");
  CHECK(scanner.next() == markup::Scanner::TT_TEXT);
  CHECK(scanner.value() == "<");
  CHECK(scanner.next() == markup::Scanner::TT_TEXT);
  CHECK(scanner.value() == ">");
  CHECK(scanner.next() == markup::Scanner::TT_EOF);
}

TEST_CASE("scan & instead of &amp;") {
  markup::instream in("Hello & other people");
  markup::Scanner scanner(in);

  CHECK(scanner.next() == markup::Scanner::TT_TEXT);
  CHECK(scanner.value() == "Hello ");
  CHECK(scanner.next() == markup::Scanner::TT_TEXT);
  CHECK(scanner.value() == "&");
  CHECK(scanner.next() == markup::Scanner::TT_TEXT);
  CHECK(scanner.value() == " other people");
  CHECK(scanner.next() == markup::Scanner::TT_EOF);
}

TEST_CASE("scan &notanentity;") {
  markup::instream in("&notanentity;");
  markup::Scanner scanner(in);

  CHECK(scanner.next() == markup::Scanner::TT_TEXT);
  CHECK(scanner.value() == "&notanentity;");
  CHECK(scanner.next() == markup::Scanner::TT_EOF);
}

TEST_CASE("scan nested elements") {
  markup::instream in("<div><p><img></p></div>");
  markup::Scanner scanner(in);

  CHECK(scanner.next() == markup::Scanner::TT_TAG_START);
  CHECK(scanner.tag() == "div");
  CHECK(scanner.next() == markup::Scanner::TT_TAG_START);
  CHECK(scanner.tag() == "p");
  CHECK(scanner.next() == markup::Scanner::TT_TAG_START);
  CHECK(scanner.tag() == "img");
  CHECK(scanner.next() == markup::Scanner::TT_TAG_END);
  CHECK(scanner.tag() == "p");
  CHECK(scanner.next() == markup::Scanner::TT_TAG_END);
  CHECK(scanner.tag() == "div");
  CHECK(scanner.next() == markup::Scanner::TT_EOF);
}

TEST_CASE("scan kitchen sink") {
  std::string html_str =
      "<div id=\"test-id\" class=\"a b c \">\n"
      "<span x-custom-attribute=\"Hello &quot;world&quot;\"><!--\n"
      "this is a comment -->this is &amp; text\n"
      "</span></div>";
  markup::instream in(html_str.data());
  markup::Scanner scanner(in);

  CHECK(scanner.next() == markup::Scanner::TT_TAG_START);
  CHECK(scanner.tag() == "div");
  CHECK(scanner.next() == markup::Scanner::TT_ATTRIBUTE);
  CHECK(scanner.attribute() == "id");
  CHECK(scanner.value() == "test-id");
  CHECK(scanner.next() == markup::Scanner::TT_ATTRIBUTE);
  CHECK(scanner.attribute() == "class");
  CHECK(scanner.value() == "a b c ");
  CHECK(scanner.next() == markup::Scanner::TT_TEXT);
  CHECK(scanner.value() == "\n");
  CHECK(scanner.next() == markup::Scanner::TT_TAG_START);
  CHECK(scanner.tag() == "span");
  CHECK(scanner.next() == markup::Scanner::TT_ATTRIBUTE);
  CHECK(scanner.attribute() == "x-custom-attribute");
  CHECK(scanner.value() == "Hello &quot;world&quot;");  // We do not decode entities in attributes
  CHECK(scanner.next() == markup::Scanner::TT_COMMENT_START);
  CHECK(scanner.next() == markup::Scanner::TT_DATA);
  CHECK(scanner.value() == "\nthis is a comment ");
  CHECK(scanner.next() == markup::Scanner::TT_COMMENT_END);
  CHECK(scanner.next() == markup::Scanner::TT_TEXT);
  CHECK(scanner.value() == "this is ");
  CHECK(scanner.next() == markup::Scanner::TT_TEXT);
  CHECK(scanner.value() == "&");
  CHECK(scanner.next() == markup::Scanner::TT_TEXT);
  CHECK(scanner.value() == " text\n");
  CHECK(scanner.next() == markup::Scanner::TT_TAG_END);
  CHECK(scanner.tag() == "span");
  CHECK(scanner.next() == markup::Scanner::TT_TAG_END);
  CHECK(scanner.tag() == "div");
  CHECK(scanner.next() == markup::Scanner::TT_EOF);
}

TEST_CASE("test long text (#273)") {
  std::string test_str;
  for (size_t i = 0; i < 1024; ++i) test_str.append("testing ");

  markup::instream in(test_str.data());
  markup::Scanner scanner(in);

  CHECK(scanner.next() == markup::Scanner::TT_TEXT);
  CHECK(scanner.value() == test_str);
  CHECK(scanner.next() == markup::Scanner::TT_EOF);
}

TEST_CASE("scan self-closing element") {
  markup::instream in("before <img src=\"#\"/> after");
  markup::Scanner scanner(in);

  CHECK(scanner.next() == markup::Scanner::TT_TEXT);
  CHECK(scanner.value() == "before ");
  CHECK(scanner.next() == markup::Scanner::TT_TAG_START);
  CHECK(scanner.tag() == "img");
  CHECK(scanner.next() == markup::Scanner::TT_ATTRIBUTE);
  CHECK(scanner.attribute() == "src");
  CHECK(scanner.value() == "#");
  CHECK(scanner.next() == markup::Scanner::TT_TAG_END);
  CHECK(scanner.tag() == "img");
  CHECK(scanner.next() == markup::Scanner::TT_TEXT);
  CHECK(scanner.value() == " after");
  CHECK(scanner.next() == markup::Scanner::TT_EOF);
}

TEST_CASE("scan script") {
  markup::instream in("<script async>true && document.body.length > 10</script>");
  markup::Scanner scanner(in);

  CHECK(scanner.next() == markup::Scanner::TT_TAG_START);
  CHECK(scanner.tag() == "script");
  CHECK(scanner.next() == markup::Scanner::TT_ATTRIBUTE);
  CHECK(scanner.attribute() == "async");
  CHECK(scanner.value() == "");
  CHECK(scanner.next() == markup::Scanner::TT_DATA);
  CHECK(scanner.value() == "true && document.body.length > 10");
  CHECK(scanner.next() == markup::Scanner::TT_TAG_END);
  CHECK(scanner.next() == markup::Scanner::TT_EOF);
}

TEST_CASE("scan style") {
  markup::instream in("<style>body { background: url(test.png); }</style>");
  markup::Scanner scanner(in);

  CHECK(scanner.next() == markup::Scanner::TT_TAG_START);
  CHECK(scanner.tag() == "style");
  CHECK(scanner.next() == markup::Scanner::TT_DATA);
  CHECK(scanner.value() == "body { background: url(test.png); }");
  CHECK(scanner.next() == markup::Scanner::TT_TAG_END);
  CHECK(scanner.next() == markup::Scanner::TT_EOF);
}

TEST_CASE("scan processing instruction") {
  // Based on https://searchfox.org/mozilla-central/source/dom/base/nsContentUtils.cpp#8961
  // element.outerHTML can produce processing instructions in the html. These
  // should be treated similar to <!-- foo -->.
  markup::instream in("<?xml version=\"1.0\"?>");
  markup::Scanner scanner(in);

  CHECK(scanner.next() == markup::Scanner::TT_PROCESSING_INSTRUCTION_START);
  CHECK(scanner.next() == markup::Scanner::TT_DATA);
  CHECK(scanner.value() == "xml version=\"1.0\"");
  CHECK(scanner.next() == markup::Scanner::TT_PROCESSING_INSTRUCTION_END);
  CHECK(scanner.next() == markup::Scanner::TT_EOF);
}