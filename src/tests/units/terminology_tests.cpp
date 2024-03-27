#include <string>

#include "catch.hpp"
#include "translator/terminology.h"

using namespace marian::bergamot;

TEST_CASE("Test preference for longer keys") {
  TerminologyMap terms{
      {"house", "short"},
      {"houseparty", "long"},
  };
  std::string str = "We should go to their houseparty tonight.";
  std::string expected = "We should go to their long tonight.";
  std::string out = ReplaceTerminology(str, terms);
  CHECK(out == expected);
}

TEST_CASE("Test multiple occurrences") {
  TerminologyMap terms{
      {"house", "1"},
      {"boat", "2"},
  };
  std::string str = "I like my house but I live on a boathouse.";
  std::string expected = "I like my 1 but I live on a 21.";
  std::string out = ReplaceTerminology(str, terms);
  CHECK(out == expected);
}

TEST_CASE("Test hit at the very beginning") {
  TerminologyMap terms{
      {"house", "1"},
      {"boat", "2"},
  };
  std::string str = "houseparty";
  std::string expected = "1party";
  std::string out = ReplaceTerminology(str, terms);
  CHECK(out == expected);
}

TEST_CASE("Test hit at the very end") {
  TerminologyMap terms{
      {"house", "1"},
      {"boat", "2"},
  };
  std::string str = "partyboat";
  std::string expected = "party2";
  std::string out = ReplaceTerminology(str, terms);
  CHECK(out == expected);
}

TEST_CASE("Test replacement of a is key of b") {
  TerminologyMap terms{
      {"rock", "paper"},
      {"paper", "scissors"},
  };
  std::string str = "rockpaper";
  std::string expected = "paperscissors";
  std::string out = ReplaceTerminology(str, terms);
  CHECK(out == expected);
}
