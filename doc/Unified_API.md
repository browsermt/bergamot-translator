# Unified (C++) API of Bergamot Translator

/* A Translation model interface for translating a plain utf-8 encoded text (without any markups and emojis). The model supports translation from 1 source language to 1 target language. There can be different implementations of this interface. */

class **AbstractTranslationModel** {

    public:

	AbstractTranslationModel();

	virtual ~AbstractTranslationModel() {};

	/* This method performs translation on a list of (utf-8) texts and returns a list of results in the same order. Each text entry can either be a word, a phrase, a sentence or a list of sentences and should contain plain text (without any markups or emojis). Additional information related to the translated text can be requested via TranslationRequest which is applied equally to each text entry. The translated text corresponding to each text entry and the additional information (as specified in the TranslationRequest) is encapsulated and returned in TranslationResult.
	The API splits each text entry into sentences internally, which are then translated independent of each other. The translated sentences are then joined together and returned in TranslationResult.
	Please refer to the TranslationRequest class to find out what additional information can be requested. The alignment information can only be requested if the model supports it (check isAlignmentSupported() API).
	*/
	virtual std::vector<std::future<TranslationResult>> translate(std::vector<std::string> texts, TranslationRequest request) = 0;

	/* Check if the model can provide alignment information b/w original and translated text. */
	virtual bool isAlignmentSupported() const = 0;
}

/* This class specifies the additional information related to the translated text (e.g. quality of the translation etc.) that can be requested to be included in the TranslationResult. These optional requests are set/unset independent of each other i.e. setting any one of them doesn’t have the side effect of setting any of the others. */

class **TranslationRequest** {

    private:

	// Optional request. The granularity for which Quality scores of the translated text will be included in TranslationResult. By default (QualityScoreGranularity::NONE), scores are not included.
	QualityScoreGranularity qualityScore = QualityScoreGranularity::NONE;

	// Optional request. The type of the alignment b/w original and translated text that will be included in TranslationResult. By default (AlignmentType::NONE), alignment is not included.
	AlignmentType alignmentType = AlignmentType::NONE;

	// Optional request. A true/false value will include/exclude the original text in the TranslationResult. By default (false), the original text is not included.
	bool includeOriginalText = false;

	// Optional request. A true/false value will include/exclude the information regarding how individual sentences of original text map to corresponding translated sentences in joined translated text in the TranslationResult. By default (false), this information is not included.
	bool includeSentenceMapping = false;

    public:

	explicit TranslationRequest();

	~TranslationRequest();

	/* Set the granularity for which the Quality scores of translated text should be included in the TranslationResult. By default (QualityScoreGranularity::NONE), scores are not included. */
	void setQualityScoreGranularity(QualityScoreGranularity granularity);

	/* Set the type of Alignment b/w original and translated text to be included in the TranslationResult. By default (AlignmentType::NONE), alignment is not included. */
	void setAlignmentType(AlignmentType alignmentType);

	/* Set to true/false to include/exclude the original text in the TranslationResult. By default (false), the original text is not included. */
	void includeOriginalText(bool originalText);

	/* Set to true/false to include/exclude the information regarding how individual sentences of original text map to corresponding translated sentences in joined translated text in the TranslationResult. By default (false), this information is not included. */
	void includeSentenceMapping(bool sentenceMapping);

	/* Return the granularity for which the Quality scores of the translated text will be included in TranslationResult. QualityScoreGranularity::NONE means the scores will not be included. */
	QualityScoreGranularity getQualityScoreGranularity() const;

	/* Return the type of Alignment b/w original and translated text that should be included in the TranslationResult. AlignmentType::NONE means the alignment will not be included. */
	AlignmentType getAlignmentType() const;

	/* Return whether the original text should be included in the TranslationResult. False means the original text will not be included. */
	bool includeOriginalText() const;

	/* Return whether the information regarding how individual sentences of original text map to corresponding translated sentences in joined translated text should be included in the TranslationResult. False means this information will not be included. */
	bool includeSentenceMapping() const;
}

/* This class represents the result of translation on a TranslationRequest. */

class **TranslationResult** {

    private:

	// Original text (utf-8) that was supposed to be translated; An optional result (it will be an empty string if not requested in TranslationRequest).
	std::string originalText;

	// Translation (in utf-8 format) of the originalText
	std::string translatedText;

	// Quality score of the translated text at the granularity specified in TranslationRequest; An optional result (it will have no information if not requested in TranslationRequest)
	QualityScore qualityScore;

	// Alignment information b/w original and translated text for AlignmentType specified in TranslationRequest; An optional result (it will have no information if not requested in TranslationRequest)
	Alignment alignment;

	// Information regarding how individual sentences of originalText map to corresponding translated sentences
        // in joined translated text (translatedText); An optional result (it will be empty if not requested in TranslationRequest);
        //       An example:
        //       originalText (contains 2 sentences)              = "What is your name? My name is Abc."
        //       translatedText (contains 2 translated sentences) = "Was ist dein Name? Mein Name ist Abc."
        //       sentenceMappings = [
        //                   {"What is your name?", "Was ist dein Name?"},  // A pair of Sentence 1 of originalText (originalText[0]) and corresponding translated sentence in translatedText (translatedText[0])
        //                   {"My name is Abc", "Mein Name ist Abc."}       // A pair of Sentence 2 of originalText (originalText[1]) and corresponding translated sentence in translatedText (translatedText[1])
        //                 ]
        //
	std::vector<std::pair<std::string_view, std::string_view>> sentenceMappings;

    public:
	// ToDo: Public Methods
}

/* This class encapsulates the configuration that is required by a translation model to perform translation. This configuration includes a path to the model file, source language vocabulary file, target language vocabulary file along with other options. */

class **TranslationModelConfiguration** {

    private:

	// Path to the translation model file
	const std::string modelPath;

	// Path to the source vocabulary file to be used by the model
	const std::string sourceLanguageVocabPath;

	// Path to the target vocabulary file to be used by the model	
	const std::string targetLanguageVocabPath;

	// ToDo: Add all possible user configurable options (e.g. min batch size, max batch size) that are relevant for translation

    public:

	// Provide the path to the model file along with the source and target vocabulary files
	TranslationModelConfiguration(const std::string& modelFilePath,
					const std::string& sourceVocabPath,
					const std::string& targetVocabPath);

	// Return the path of the model file
	const std::string& getModelFilePath() const;

	// Return the path of the source language vocabulary file
	const std::string& getSourceVocabularyPath() const;

	// Return the path of the target language vocabulary file
	const std::string& getSourceVocabularyPath() const;
}

// All possible granularities for which Quality Scores can be returned for translated (utf-8) text

enum class QualityScoreGranularity {

	WORD,
	SENTENCE,
	NONE,
}

// All possible supported alignment types between a text and its translation

enum class AlignmentType {

	SOFT,
	NONE,
}

// This class represents the Quality Scores for various spans of the translated text at a specific granularity

class QualityScore {

      private:

	// Sections of a text for the Quality Scores
	std::vector<std::string_view> textViews;

	// Quality Scores corresponding to each section of the text in textViews in the same order
	std::vector<float> textScores;

        // Granularity of the text for the Quality scores above
	QualityScoreGranularity textGranularity;

      public:
	// ToDo: Public Methods
}

// This class encapsulates a translated text, all the sections of the original text that align to this translated text and the corresponding alignments for each of these sections of original text.

class Alignment {

      private:

        // A list of sections of a translated text
        // An example: originalText   = "What do you need"
        //             translatedText = "Was brauchst du"
        //             translatedTextViews = ["Was ", "brauchst", "du"]
	std::vector<std::string_view> translatedTextViews;

        // Each ith entry of this container corresponds to a list of all the sections of the original text that align to the ith entry of translatedTextView
        // For the example above:
        //             translatedTextViews = ["Was ", "brauchst", "du"]
        //             originalTextViews   = [
        //                                      ["What"],         // originalTextViews[0] = All sections of original text that align with translatedTextViews[0] i.e. "Was"
        //                                      ["you", "need"],  // originalTextViews[1] = All sections of original text that align with translatedTextViews[1] i.e. "brauchst"
        //                                      ["you"]           // originalTextViews[2] = All sections of original text that align with translatedTextViews[2] i.e. "du"
        //                                   ]
	std::vector<std::vector<std::string_view>> originalTextViews;

        // Each ith entry of this container corresponds to the alignments of all the sections of the original text (ith entry of originalTextViews) that align to the ith entry of translatedTextViews
        // For the example above:
        //             alignments          = [
        //                                      [0.90],         // alignments[0] = Alignments of all sections of original text (i.e. originalTextViews[0]) to translatedTextViews[0] i.e. "Was"
        //                                      [0.3, 0.7],     // alignments[1] = Alignments of all sections of original text (i.e. originalTextViews[1]) to translatedTextViews[1] i.e. "brauchst"
        //                                      [0.9]           // alignments[2] = Alignments of all sections of original text (i.e. originalTextViews[2]) to translatedTextViews[2] i.e. "du"
        //                                   ]
	std::vector<std::vector<float>> alignments;

        // Type of the alignment b/w original and translated text above
	AlignmentType alignmentType;

      public:
	// ToDo: Public Methods
}
