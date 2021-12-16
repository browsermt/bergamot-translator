#include <string>

#include "catch.hpp"
#include "translator/xh_scanner.h"

TEST_CASE("scan element with attributes") {
  markup::instream in("<div id=\"test\" class=\"a b c \" hidden>");
  markup::scanner scanner(in);

  CHECK(scanner.next_token() == markup::scanner::TT_TAG_START);
  CHECK(scanner.tag_name() == "div");
  CHECK(scanner.next_token() == markup::scanner::TT_ATTR);
  CHECK(scanner.attr_name() == "id");
  CHECK(scanner.value() == "test");

  CHECK(scanner.next_token() == markup::scanner::TT_ATTR);
  CHECK(scanner.attr_name() == "class");
  CHECK(scanner.value() == "a b c ");

  CHECK(scanner.next_token() == markup::scanner::TT_ATTR);
  CHECK(scanner.attr_name() == "hidden");
  CHECK(scanner.value() == "");

  CHECK(scanner.next_token() == markup::scanner::TT_EOF);
}

TEST_CASE("scan element with text") {
  markup::instream in("<span>Hello world</span>");
  markup::scanner scanner(in);

  CHECK(scanner.next_token() == markup::scanner::TT_TAG_START);
  CHECK(scanner.next_token() == markup::scanner::TT_TEXT);
  CHECK(scanner.value() == "Hello world");
  CHECK(scanner.next_token() == markup::scanner::TT_TAG_END);
  CHECK(scanner.next_token() == markup::scanner::TT_EOF);
}

TEST_CASE("scan html entities") {
  markup::instream in("Hello &amp; &apos;world&apos;");
  markup::scanner scanner(in);

  CHECK(scanner.next_token() == markup::scanner::TT_TEXT);
  CHECK(scanner.value() == "Hello ");
  CHECK(scanner.next_token() == markup::scanner::TT_TEXT);
  CHECK(scanner.value() == "&");
  CHECK(scanner.next_token() == markup::scanner::TT_TEXT);
  CHECK(scanner.value() == " ");
  CHECK(scanner.next_token() == markup::scanner::TT_TEXT);
  CHECK(scanner.value() == "'");
  CHECK(scanner.next_token() == markup::scanner::TT_TEXT);
  CHECK(scanner.value() == "world");
  CHECK(scanner.next_token() == markup::scanner::TT_TEXT);
  CHECK(scanner.value() == "'");
  CHECK(scanner.next_token() == markup::scanner::TT_EOF);
}

TEST_CASE("scan nested elements") {
  markup::instream in("<div><p><img></p></div>");
  markup::scanner scanner(in);

  CHECK(scanner.next_token() == markup::scanner::TT_TAG_START);
  CHECK(scanner.tag_name() == "div");
  CHECK(scanner.next_token() == markup::scanner::TT_TAG_START);
  CHECK(scanner.tag_name() == "p");
  CHECK(scanner.next_token() == markup::scanner::TT_TAG_START);
  CHECK(scanner.tag_name() == "img");
  CHECK(scanner.next_token() == markup::scanner::TT_TAG_END);
  CHECK(scanner.tag_name() == "p");
  CHECK(scanner.next_token() == markup::scanner::TT_TAG_END);
  CHECK(scanner.tag_name() == "div");
  CHECK(scanner.next_token() == markup::scanner::TT_EOF);
}

TEST_CASE("scan kitchen sink") {
  std::string html_str =
      "<div id=\"test-id\" class=\"a b c \">\n"
      "<span x-custom-attribute=\"Hello &quot;world&quot;\"><!--\n"
      "this is a comment -->this is &amp; text\n"
      "</span></div>";
  markup::instream in(html_str.data());
  markup::scanner scanner(in);

  CHECK(scanner.next_token() == markup::scanner::TT_TAG_START);
  CHECK(scanner.tag_name() == "div");
  CHECK(scanner.next_token() == markup::scanner::TT_ATTR);
  CHECK(scanner.attr_name() == "id");
  CHECK(scanner.value() == "test-id");
  CHECK(scanner.next_token() == markup::scanner::TT_ATTR);
  CHECK(scanner.attr_name() == "class");
  CHECK(scanner.value() == "a b c ");
  CHECK(scanner.next_token() == markup::scanner::TT_TEXT);
  CHECK(scanner.value() == "\n");
  CHECK(scanner.next_token() == markup::scanner::TT_TAG_START);
  CHECK(scanner.tag_name() == "span");
  CHECK(scanner.next_token() == markup::scanner::TT_ATTR);
  CHECK(scanner.attr_name() == "x-custom-attribute");
  CHECK(scanner.value() == "Hello &quot;world&quot;");  // We do not decode entities in attributes
  CHECK(scanner.next_token() == markup::scanner::TT_COMMENT_START);
  CHECK(scanner.next_token() == markup::scanner::TT_DATA);
  CHECK(scanner.value() == "\nthis is a comment ");
  CHECK(scanner.next_token() == markup::scanner::TT_COMMENT_END);
  CHECK(scanner.next_token() == markup::scanner::TT_TEXT);
  CHECK(scanner.value() == "this is ");
  CHECK(scanner.next_token() == markup::scanner::TT_TEXT);
  CHECK(scanner.value() == "&");
  CHECK(scanner.next_token() == markup::scanner::TT_TEXT);
  CHECK(scanner.value() == " text\n");
  CHECK(scanner.next_token() == markup::scanner::TT_TAG_END);
  CHECK(scanner.tag_name() == "span");
  CHECK(scanner.next_token() == markup::scanner::TT_TAG_END);
  CHECK(scanner.tag_name() == "div");
  CHECK(scanner.next_token() == markup::scanner::TT_EOF);
}

TEST_CASE("test long text (#273)") {
  std::string test_str;
  for (size_t i = 0; i < 1024; ++i) test_str.append("testing ");

  markup::instream in(test_str.data());
  markup::scanner scanner(in);

  CHECK(scanner.next_token() == markup::scanner::TT_TEXT);
  CHECK(scanner.value() == test_str);
  CHECK(scanner.next_token() == markup::scanner::TT_EOF);
}