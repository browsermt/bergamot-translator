#include <vector>

#include "html_tests.h"
#include "data/types.h" // for marian::string_view
#include "translator/html.h"
#include "translator/response.h"
#include "catch.hpp"

using namespace marian::bergamot;
using marian::string_view;

std::ostream &operator<<(std::ostream &out, std::pair<ByteRange,ByteRange> const &b) {
	return out << '(' << b.first << ',' << b.second << ')';
}

std::ostream &operator<<(std::ostream &out, ByteRange const &b) {
	return out << '{' << b.begin << ',' << b.end << '}';
}

std::vector<ByteRange> AsByteRanges(Annotation const &annotation) {
	std::vector<ByteRange> words;
	for (std::size_t sentenceIdx = 0; sentenceIdx < annotation.numSentences(); ++sentenceIdx)
		for (std::size_t wordIdx = 0; wordIdx < annotation.numWords(sentenceIdx); ++wordIdx)
			words.push_back(annotation.word(sentenceIdx, wordIdx));
	return words;
}

std::vector<std::string> AsWords(AnnotatedText const &annotation) {
	std::vector<std::string> words;
	for (std::size_t sentenceIdx = 0; sentenceIdx < annotation.numSentences(); ++sentenceIdx)
		for (std::size_t wordIdx = 0; wordIdx < annotation.numWords(sentenceIdx); ++wordIdx)
			words.emplace_back(annotation.word(sentenceIdx, wordIdx));
	return words;
}

void RecordSentenceFromByteRange(AnnotatedText &text, std::vector<ByteRange> const &ranges) {
	assert(ranges.size() > 0);

	std::vector<string_view> tokens;
	tokens.reserve(ranges.size());

	for (auto &&range : ranges)
		tokens.emplace_back(text.text.data() + range.begin, range.size());

	text.recordExistingSentence(tokens.begin(), tokens.end(), text.text.data() + ranges[0].begin);
}

TEST_CASE("Test identifying text spans") {
	std::string html_code("<p>Hello <b>world</b></p>\n");

	std::vector<std::pair<ByteRange,ByteRange>> spans{
		std::make_pair(ByteRange{ 3, 3+6}, ByteRange{ 0,   6}), // Hello_
		std::make_pair(ByteRange{12,12+5}, ByteRange{ 6, 6+5}), // world
		std::make_pair(ByteRange{25,25+1}, ByteRange{11,11+1}) // \n
	};

	HTML html(std::move(html_code), true);
	CHECK(html.spans() == spans);
}

TEST_CASE("Test reconstruction") {
	std::string input("<p><input>H<u>e</u>llo <b>world</b> how <u>are you</u>?</p>\n");
	
	std::vector<std::pair<ByteRange,ByteRange>> spans{
		std::make_pair(ByteRange{10,10+1}, ByteRange{ 0,   1}), // H
		std::make_pair(ByteRange{14,14+1}, ByteRange{ 1, 1+1}), // e
		std::make_pair(ByteRange{19,19+4}, ByteRange{ 2, 2+4}), // llo_
		std::make_pair(ByteRange{26,26+5}, ByteRange{ 6, 6+5}), // world
		std::make_pair(ByteRange{35,35+5}, ByteRange{11,11+5}), // _how_
		std::make_pair(ByteRange{43,43+7}, ByteRange{16,16+7}), // are you
		std::make_pair(ByteRange{54,54+1}, ByteRange{23,23+1}), // ?
		std::make_pair(ByteRange{59,59+1}, ByteRange{24,24+1})  // \n
	};

	std::string text(input);
	HTML html(std::move(text), true); // TODO: move, but really a reference?
	CHECK(html.spans() == spans);
	CHECK(text == "Hello world how are you?\n");

	AnnotatedText source(std::move(text));
	std::vector<string_view> tokens{
		string_view(source.text.data() +  0, 4), // Hell
		string_view(source.text.data() +  4, 1), // o
		string_view(source.text.data() +  5, 6), // _world
		string_view(source.text.data() + 11, 4), // _how
		string_view(source.text.data() + 15, 4), // _are
		string_view(source.text.data() + 19, 4), // _you
		string_view(source.text.data() + 23, 1), // ?
		string_view(source.text.data() + 24, 0), // "\n" (but 0 length?)
	};

	source.recordExistingSentence(tokens.begin(), tokens.end(), source.text.data());

	Response response;
	response.source = source;

	html.Restore(response);
	CHECK(response.source.text == input);

	std::vector<ByteRange> restored_tokens{
		ByteRange{ 0,  0+21}, // <p><input>H<u>e</u>ll
		ByteRange{21, 21+ 1}, // o
		ByteRange{22, 22+ 9}, // _<b>world
		ByteRange{31, 31+ 8}, // </b>_how
		ByteRange{39, 39+ 7}, // _<u>are
		ByteRange{46, 46+ 4}, // _you
		ByteRange{50, 50+ 5}, // </u>?
		ByteRange{55, 55+ 4}, // "</p>\n" (but 0 length for newline?)
	};
	CHECK(response.source.text.size() == restored_tokens.back().end + 1); // 59 + \n
	CHECK(response.source.annotation.numSentences() == 1);
	CHECK(response.source.annotation.numWords(0) == restored_tokens.size());
	CHECK(AsByteRanges(response.source.annotation) == restored_tokens);

	// Same test as above, but easier to read. Will use this further down.
	std::vector<std::string> restored_tokens_str{
		"<p><input>H<u>e</u>ll",
		"o",
		" <b>world",
		"</b> how",
		" <u>are",
		" you",
		"</u>?",
		"</p>" // (TODO no newline?)
	};

	CHECK(AsWords(response.source) == restored_tokens_str);


	/*
	std::vector<ByteRange> reconstructed_tokens;
	
	auto span_it = spans.begin();
	auto prev_it = spans.end();
	std::size_t offset = 0;
	for (auto token_it = tokens.begin(); token_it != tokens.end(); ++token_it) {
		ByteRange token{
			static_cast<std::size_t>(token_it->data() - source.text.data()),
			static_cast<std::size_t>(token_it->data() - source.text.data()) + token_it->size()
		};

		ByteRange reconstructed{
			token.begin + offset,
			token.end + offset
		};

		while (
			span_it != spans.end()
			&& span_it->second.begin >= token.begin
			&& span_it->second.begin < token.end 
		) {
			// std::cerr << "Adding "
			// 				  << (span_it - spans.begin()) << " to " << (token_it - tokens.begin())
			// 				  << " because span " << *span_it << " falls in token " << token
			// 				  << std::endl;
			std::size_t additional_html = span_it->first.begin - (prev_it == spans.end() ? 0 : prev_it->first.end);
			reconstructed.end += additional_html;
			offset += additional_html;
			prev_it = span_it++;
		}

		reconstructed_tokens.push_back(reconstructed);
	}

	if (span_it != spans.end()) {
		std::size_t additional_html = span_it->first.begin - (prev_it == spans.end() ? 0 : prev_it->first.end);
		reconstructed_tokens.back().end += additional_html;
	}

	CHECK(restored_tokens == reconstructed_tokens);
	*/
}

TEST_CASE("Test reconstruction of multiple sentences") {
	std::string input("<p>This <em>is a sentence. And so is</em> this.</p>\n");

	HTML html(std::move(input), true);
	CHECK(input == "This is a sentence. And so is this.\n");

	Response response;
	response.source = AnnotatedText(std::move(input));

	RecordSentenceFromByteRange(response.source, {
		ByteRange{ 0,  4}, // 0.0 "This"
		ByteRange{ 4,  7}, // 0.1 " is"
		ByteRange{ 7,  9}, // 0.2 " a"
		ByteRange{ 9, 18}, // 0.3 " sentence"
		ByteRange{18, 19}, // 0.4 "."
		ByteRange{19, 19}  // 0.5 ""
	});

	RecordSentenceFromByteRange(response.source, {
		ByteRange{20, 23}, // 1.0 "And"
		ByteRange{23, 26}, // 1.1 " so"
		ByteRange{26, 29}, // 1.2 " is"
		ByteRange{29, 34}, // 1.3 " this"
		ByteRange{34, 35}, // 1.4 "."
		ByteRange{35, 35}  // 1.5 ""
	});

	std::vector<std::string> tokens{
		"This", " is", " a", " sentence", ".", "",
		"And", " so", " is", " this", ".", ""
	};

	CHECK(AsWords(response.source) == tokens);

	html.Restore(response);

	std::vector<std::string> html_tokens{
		"<p>This", " <em>is", " a", " sentence", ".", "",
		"And", " so", " is", "</em> this", ".", "</p>"
	};

	CHECK(AsWords(response.source) == html_tokens);
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
		ByteRange{ 0,  4}, // 0.0 "This"
		ByteRange{ 4,  7}, // 0.1 " is"
		ByteRange{ 7,  9}, // 0.2 " a"
		ByteRange{ 9, 18}, // 0.3 " sentence"
		ByteRange{18, 20}, // 0.4 " <"
		ByteRange{20, 24}, // 0.5 "with"
		ByteRange{24, 25}, // 0.6 ">"
		ByteRange{25, 31}, // 0.7 " named"
		ByteRange{31, 33}, // 0.8 " &"
		ByteRange{33, 42}, // 0.9 " entities"
		ByteRange{42, 42} // 0.10 ""
	});

	html.Restore(response);

	std::vector<std::string> html_tokens{
		"<p data-attr=\"&quot;&apos;\">This",
		" is",
		" a",
		" sentence",
		" &lt;", // Oh trouble! The < is completely 'consumed'
		"with",
		"&gt;"," named",
		" &amp;",
		" entities",
		"</p>"
	};

	CHECK(AsWords(response.source) == html_tokens);
}