# Unified (C++) API of Bergamot Translator

/* A Translation model interface that translates a plain utf-8 encoded text (without any markups and emojis). The model supports translation from 1 source language to 1 target language. There can be different implementations of this interface. */

class **AbstractTranslationModel** {

    public:

	/* Constructor that takes model configuration (path of the model file, source and target language vocabulary files and other options) that is required by the model to perform translation. The information provided by the configuration stays constant during the lifetime of the model instance. The model supports translation from 1 source language to 1 target language.
	*/
	AbstractTranslationModel(const TranslationModelConfiguration& config);

	virtual ~AbstractTranslationModel() = 0;

	/* This method performs translation on a list of texts (utf-8) and returns a list of results. The text to be translated along with other optional requests is provided via TranslationRequest. The translated text along with the optional information corresponding to the optional requests is returned in TranslationResult.
	The text to be translated can either be a word, a phrase, a sentence or a list of sentences and should be plain (without any markups or emojis). The API splits each text entry into sentences internally, which are then translated independent of each other and then joined together to be returned as the translation result for that entry.
	Please refer to the TranslationRequest class for all possible optional requests. The alignment information can only be requested if the model supports it (check isAlignmentSupported() API).
	*/
	virtual vector<std::future<TranslationResult>> translate(const vector<TranslationRequest>& requests) = 0;

	/* Check if the model can provide alignment information b/w original and translated text. */
	virtual boolean isAlignmentSupported() const;
}

/* A class that defines a translation request. It encapsulates the text to be translated and other optional requests. The optional requests represent the additional information related to the translated text (e.g. quality of the translation etc.). These optional requests are set/unset independent of each other i.e. setting any one of them doesn’t have the side effect of setting any of the others. */

class **TranslationRequest** {

    private:

	// The text (utf-8) to be translated
	string originalText;

	// Optional request. The granularity for which Quality scores of the translated text will be returned in TranslationResult. By default (QualityScoreGranularity::NONE), scores are not returned.
	QualityScoreGranularity qualityScore = QualityScoreGranularity::NONE;

	// Optional request. The alignment information b/w original and translated text. By default (i.e. Alignment::NONE), alignment is not returned.
	Alignment alignment = Alignment::NONE;

	// Optional request. If set to true, the original text will be included in the TranslationResult. By default (false), the original text will not be returned.
	boolean returnOriginalTextInResult = false;

	// Optional request. If set to true, mapping b/w each sentence (in the original text) and the corresponding translated sentence (in the translated text) will be included in the TranslationResult. By default (false), this info will not be included.
	boolean originalToTranslatedSentenceMapping = false;

    public:

	/* Create the translation request by providing the (utf-8) plain text (without any markups). Text can be a single word, a phrase, a sentence or multiple sentences. */
	TranslationRequest(const string& text);

	~TranslationRequest();

	/* Set the granularity for which the Quality scores of translated text should be included in the TranslationResult. */
	void setQualityScoreGranularity(QualityScoreGranularity granularity);

	/* Set the Alignment b/w original and translated text to be included in the TranslationResult. */
	void setAlignment(Alignment alignment);

	/* Set to true/false to include/exclude the original text in the TranslationResult. */
	void setOriginalTextInResult(boolean originalTextInResult);

	/* Set to true/false to include/exclude the mapping b/w each sentence (in the original text) and the corresponding translated sentence (in the joined translated text) in the TranslationResult. */
	void setOriginalToTranslatedSentenceMapping(boolean sentenceMapping);

	/* Return the granularity for which the Quality scores are requested. */
	QualityScoreGranularity getQualityScoreRequest() const;

	/* Return the alignment to be included in the TranslationResult. */
	Alignment getAlignment() const;

	/* Return whether the original text is requested to be included in the TranslationResult. */
	boolean isOriginalTextInResult() const;

	/* Return whether the mapping b/w each sentence in the original text and the corresponding translated sentence (in the joined translated text) is requested to be included in the TranslationResult. */
	boolean isOriginalToTranslatedSentenceMapping() const;
}

/* This class represents the result of translation on a TranslationRequest. */

class **TranslationResult** {

    private:

	// Original text (utf-8) that was supposed to be translated; An optional result (it will be an empty string if not requested in TranslationRequest).
	string originalText;

	// Translation (in utf-8 format) of the originalText
	string translatedText;

	// Quality score of the translated text at the granularity; An optional result (it will have no information if not requested in TranslationRequest)
	QualityScoreResult qualityScoreResult;

	// Alignment information b/w original and translated text; An optional result (it will have no information if not requested in TranslationRequest)
	AlignmentResult alignment;

	// Mapping information b/w each sentence (in originalText) and corresponding translated sentence (in translatedText); An optional result (it will have no information if not requested in TranslationRequest); ToDo: Decide TextSpan: Byte offset or CodePoint count or something else
	map<TextSpan, TextSpan> originalToTranslatedSentenceSpans;

    public:
	// ToDo: Public Methods
}

/* This class encapsulates the configuration that is required by a translation model to perform translation. This configuration includes a path to the model file, source language vocabulary file, target language vocabulary file along with other options. */

class **TranslationModelConfiguration** {

    private:

	// Path to the translation model file
	const string modelPath;

	// Path to the source vocabulary file to be used by the model
	const string sourceLanguageVocabPath;

	// Path to the target vocabulary file to be used by the model	
	const string targetLanguageVocabPath;

	// ToDo: Add all possible user configurable options (e.g. min batch size, max batch size) that are relevant for translation

    public:

	// Provide the path to the model file along with the source and target vocabulary files
	TranslationModelConfiguration(const string& modelFilePath,
	const string& sourceVocabPath,
	const string& targetVocabPath);

	// Return the path of the model file
	const string& getModelFilePath() const;

	// Return the path of the source language vocabulary file
	const string& getSourceVocabularyPath() const;

	// Return the path of the target language vocabulary file
	const string& getSourceVocabularyPath() const;
}

// All possible granularities for which Quality Scores can be returned for translated (utf-8) text

enum class QualityScoreGranularity {

	WORD = 0,
	SENTENCE,
	NONE,
}

// All possible supported alignments between a text and its translation

enum class Alignment {

	SOFT = 0,
	NONE,
}

class QualityScoreResult {

      private:

	// Container for the span of the translated text and the corresponding Quality Scores; ToDo: Decide TextSpan: Byte offset or CodePoint count or something else
	map<TextSpan, float> translatedTextSpanToQualityScore;

	// Granularity of the translated text for the Quality scores above
	QualityScoreGranularity granularity;

      public:
	// ToDo: Public Methods
}

class AlignmentResult {

      private:

	typedef map<TextSpan, float> OriginalTextSpanToAlignments;

	// Alignments b/w different spans of translated text and original text; ToDo: Decide TextSpan: Byte offset or CodePoint count or something else
	map<TextSpan, OriginalTextSpanToAlignments> translatedToOriginalTextSpanAlignments;

      public:
	// ToDo: Public Methods
}
