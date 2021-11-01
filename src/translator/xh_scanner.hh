//|
//| simple and fast XML/HTML scanner/tokenizer
//|
//| (C) Andrew Fedoniouk @ terrainformatica.com
//|

namespace markup {
    struct instream {
        const char *p;
        const char *end;
        explicit instream(const char *src) : p(src), end(src+strlen(src)) {}
        char get_char() { return p < end ? *p++ : 0; }
    };


    class scanner {
    public:
        enum token_type {
            TT_ERROR = -1,
            TT_EOF = 0,

            TT_TAG_START,   // <tag ...
            //     ^-- happens here
            TT_TAG_END,     // </tag>
            //       ^-- happens here
            // <tag ... />
            //            ^-- or here
            TT_ATTR,        // <tag attr="value" >
            //                  ^-- happens here
            TT_WORD,
            TT_SPACE,

            TT_DATA,        // content of followings:
            // (also content of TT_TAG_START and TT_TAG_END, if the tag is 'script' or 'style')

            TT_COMMENT_START, TT_COMMENT_END, // after "<!--" and "-->"
            TT_CDATA_START, TT_CDATA_END,     // after "<![CDATA[" and "]]>"
            TT_PI_START, TT_PI_END,           // after "<?" and "?>"
            TT_ENTITY_START, TT_ENTITY_END,   // after "<!ENTITY" and ">"

        };

        enum $ {
            MAX_TOKEN_SIZE = 1024, MAX_NAME_SIZE = 128
        };

    public:

        explicit scanner(instream &is) :
                value_length(0),
                tag_name_length(0),
                attr_name_length(0),
                input(is),
                input_char(0),
                got_tail(false) { c_scan = &scanner::scan_body; }

        // get next token
        token_type get_token() { return (this->*c_scan)(); }

        // get value of TT_WORD, TT_SPACE, TT_ATTR and TT_DATA
        const char *get_value();

        // get attribute name
        const char *get_attr_name();

        // get tag name
        const char *get_tag_name();

    private: /* methods */

        typedef token_type (scanner::*scan)();

        scan c_scan; // current 'reader'

        // content 'readers'
        token_type scan_body();

        token_type scan_head();

        token_type scan_comment();

        token_type scan_cdata();

        token_type scan_special();

        token_type scan_pi();

        token_type scan_tag();

        token_type scan_entity_decl();

        char skip_whitespace();

        void push_back(char c);

        char get_char();

        static bool is_whitespace(char c);

        void append_value(char c);

        void append_attr_name(char c);

        void append_tag_name(char c);

    private: /* data */

        char value[MAX_TOKEN_SIZE]{};
        unsigned int value_length;

        char tag_name[MAX_NAME_SIZE]{};
        unsigned int tag_name_length;

        char attr_name[MAX_NAME_SIZE]{};
        unsigned int attr_name_length;

        instream &input;
        char input_char;

        bool got_tail; // aux flag used in scan_comment, etc.

    };
}
