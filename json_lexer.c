#include "json_lexer.h"

#include <float.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

const char* JSON__TOKEN_STRINGS[] = {
#define X(name) #name,
    JSON_TOKEN_TYPES(X)
#undef X
};

const char* JSON__LEX_ERR_STRINGS[] = {
#define X(name) #name,
    JSON_LEX_ERR_TYPES(X)
#undef X
};

#ifdef LEX_NO_STIN
    #define LEX_STIN 
#else 
    #define LEX_STIN static inline
#endif

#define LEX_ADV_CHECK(lex, status)  \
    do { \
        lexer_advance((lex)); \
        if ((lex)->parse_point == (lex)->input_end) \
        return JSON_LEX_##status; \
    } while (0)


LEX_STIN JSON_LEX_ERR init_token(json_lexer_t* lex);

LEX_STIN void lexer_advance(json_lexer_t* lex);

LEX_STIN char lex_char(const json_lexer_t* lex);


LEX_STIN JSON_LEX_ERR lexer_number(json_lexer_t* lex);

LEX_STIN JSON_LEX_ERR lexer_int(json_lexer_t* lex);

LEX_STIN JSON_LEX_ERR lexer_digits(json_lexer_t* lex);

LEX_STIN JSON_LEX_ERR lexer_fraction(json_lexer_t* lex, bool* hasFrac);

LEX_STIN JSON_LEX_ERR lexer_exponent(json_lexer_t* lex, bool* hasExp);


LEX_STIN JSON_LEX_ERR lexer_string(json_lexer_t* lex);

LEX_STIN JSON_LEX_ERR lexer_escape_seq(json_lexer_t* lex);


LEX_STIN JSON_LEX_ERR lexer_keyword(json_lexer_t* lex);


LEX_STIN JSON_LEX_ERR lexer_symbol(json_lexer_t* lex);


LEX_STIN bool is_whitespace(char c);

LEX_STIN bool is_digit(char c);

LEX_STIN bool is_number_start(char c);

LEX_STIN bool is_string_start(char c);

LEX_STIN bool is_control_char(char c);

LEX_STIN bool is_hex_digit(char c);

LEX_STIN bool is_keyword_start(char c);

LEX_STIN bool is_escape_char(char c);


void json_lexer_init(json_lexer_t* lex, const char* input_start, const char* input_end) {

    lex->input_start = input_start;
    lex->input_end = input_end;
    lex->parse_point = input_start;

    lex->line = lex->col = 1;
    lex->last_status = JSON_LEX_OK;

    lex->token = (json_token_t) {0};
}

JSON_LEX_ERR json_lexer_next_token(json_lexer_t* lex) {
    // each function call ends up a byte after the last token
    // if an error occured, the parse point is exactly where it stopped

    for(;;) {
        if (lex->parse_point == lex->input_end) {
            return lex->last_status = JSON_LEX_EOF;
        }

        if (!is_whitespace(lex_char(lex))) {
            break;
        }

        lexer_advance(lex);
    }

    char c = lex_char(lex);
    if (is_number_start(c)) {
        return lex->last_status = lexer_number(lex);

    } else if (is_string_start(c)) {
        return lex->last_status = lexer_string(lex);

    } else if (is_keyword_start(c)) {
        return lex->last_status = lexer_keyword(lex);

    } else {
        return lex->last_status = lexer_symbol(lex);

    } 
}

LEX_STIN JSON_LEX_ERR lexer_number(json_lexer_t* lex) {
    JSON_LEX_ERR st = init_token(lex);
    if(!st)
        return st;

    bool hasFrac = false, hasExp = false;

    // validate json number format
    st = lexer_int(lex);
    if (st == JSON_LEX_OK)
        st = lexer_fraction(lex, &hasFrac);

    if (st == JSON_LEX_OK)
        st = lexer_exponent(lex, &hasExp);

    if (st != JSON_LEX_OK && st != JSON_LEX_EOF)
        return st;

    size_t strLen = (size_t)(lex->parse_point - lex->token.start);
    lex->token.strLen = (uint32_t) strLen;

    if (hasFrac || hasExp) {
        // this is a slowdown, scince its getting reparsed
        // but its much simpler
        errno = 0;
        double dblVal = strtod(lex->token.start, NULL);
        if (errno == ERANGE) {
            return JSON_LEX_ERR_NUM_RANGE;
        }

        lex->token.type = JSON_TOKEN_DOUBLE;
        lex->token.number.dblVal = dblVal;
        return JSON_LEX_OK;

    } else {
        errno = 0;
        int64_t intVal = strtoll(lex->token.start , NULL, 10);
        if (errno == ERANGE) {
            return JSON_LEX_ERR_NUM_RANGE;
        }

        lex->token.type = JSON_TOKEN_INT;
        lex->token.number.intVal = intVal;
        return JSON_LEX_OK;
    }

    return JSON_LEX_ERR_NUM_RANGE;
}

LEX_STIN JSON_LEX_ERR lexer_string(json_lexer_t* lex) {
    JSON_LEX_ERR st = init_token(lex);
    if(!st)
        return st;

    LEX_ADV_CHECK(lex, ERR_STR_UNTERMINATED);

    char c;
    for (;;) {
        c = lex_char(lex);
        if (c == '"') {
            lexer_advance(lex);
            break;
        }

        if (is_control_char(c)) {
            return JSON_LEX_ERR_STR_CONTROL_CHAR;
        }

        if (c == '\\') {
            LEX_ADV_CHECK(lex, ERR_STR_UNTERMINATED);
            JSON_LEX_ERR st = lexer_escape_seq(lex);
            if (st) return st;
            continue;
        }

        LEX_ADV_CHECK(lex, ERR_STR_UNTERMINATED);
    }

    size_t strLen = (size_t)(lex->parse_point - lex->token.start);
    if (strLen >= UINT32_MAX)
        return JSON_LEX_ERR_STR_TOO_LONG;

    lex->token.strLen = (uint32_t) strLen;
    lex->token.type = JSON_TOKEN_STRING;

    return JSON_LEX_OK;
}

LEX_STIN JSON_LEX_ERR lexer_keyword(json_lexer_t* lex) {
    JSON_LEX_ERR st = init_token(lex);
    if(!st)
        return st;

    const char* p = lex->parse_point;
    size_t left = (size_t)(lex->input_end - p);

    if (left >= 4 && strncmp(p, "true", 4) == 0) {
        lex->token.type = JSON_TOKEN_TRUE;
        lex->token.strLen = 4;
        lex->parse_point += 4;
        lex->col += 4;
        return JSON_LEX_OK;
    }

    if (left >= 5 && strncmp(p, "false", 5) == 0) {
        lex->token.type = JSON_TOKEN_FALSE;
        lex->token.strLen = 5;
        lex->parse_point += 5;
        lex->col += 5;
        return JSON_LEX_OK;
    }

    if (left >= 4 && strncmp(p, "null", 4) == 0) {
        lex->token.type = JSON_TOKEN_NULL;
        lex->token.strLen = 4;
        lex->parse_point += 4;
        lex->col += 4;
        return JSON_LEX_OK;
    }

    return JSON_LEX_ERR_INVALID_KEYWORD;
}

LEX_STIN JSON_LEX_ERR lexer_symbol(json_lexer_t* lex) {
    JSON_LEX_ERR st = init_token(lex);
    if(!st)
        return st;

    char c = lex_char(lex);
    switch (c) {
    case '{': lex->token.type = JSON_TOKEN_LBRACE; break;
    case '}': lex->token.type = JSON_TOKEN_RBRACE; break;
    case '[': lex->token.type = JSON_TOKEN_LBRACKET; break;
    case ']': lex->token.type = JSON_TOKEN_RBRACKET; break;
    case ':': lex->token.type = JSON_TOKEN_COLON; break;
    case ',': lex->token.type = JSON_TOKEN_COMMA; break;
    default: return JSON_LEX_ERR_UNEXPECTED_SYMBOL;
    }

    lex->token.strLen = 1;
    lexer_advance(lex);

    return JSON_LEX_OK;
}

void json_lexer_recover_to_last_token(json_lexer_t *lex) {
    lex->parse_point = lex->token.start;
    lex->line = lex->token.line;
    lex->col = lex->token.col;
    lex->last_status = JSON_LEX_OK;
}

LEX_STIN char lex_char(const json_lexer_t* lex) {
    return *lex->parse_point;
}

LEX_STIN void lexer_advance(json_lexer_t* lex) {

    if (lex_char(lex) == '\n') {
        ++lex->line;
        lex->col = 1;
    } else {
        ++lex->col;
    }

    ++lex->parse_point;
}

LEX_STIN JSON_LEX_ERR init_token(json_lexer_t* lex) {
    lex->token.start = lex->parse_point;
    lex->token.type = JSON_TOKEN_INVALID;
    lex->token.number.intVal = 0;
    lex->token.strLen = 0;

    if (lex->line >= UINT32_MAX)
        return JSON_LEX_ERR_LINE_OVERFLOW;

    lex->token.line = (uint32_t) lex->line;

    if (lex->col >= UINT32_MAX)
        return JSON_LEX_ERR_COL_OVERFLOW;

    lex->token.col = (uint32_t) lex->col;

    return JSON_LEX_OK;
}

LEX_STIN JSON_LEX_ERR lexer_int(json_lexer_t* lex) {
    char c = lex_char(lex);

    if (c == '-') {
        LEX_ADV_CHECK(lex, ERR_NUM_UNTERMINATED);

        c = lex_char(lex);
        if (!is_digit(c))
            return JSON_LEX_ERR_NUM_EXPECT_DIGIT;
    }

    if (c == '0') {
        LEX_ADV_CHECK(lex, EOF);

        if (is_digit(lex_char(lex))) // JSON: no leading zeros
            return JSON_LEX_ERR_NUM_LEADING_ZERO;

        return JSON_LEX_OK; // Next could be fraction or exponent
    }

    return lexer_digits(lex);
}

LEX_STIN JSON_LEX_ERR lexer_digits(json_lexer_t* lex) {
    if (!is_digit(lex_char(lex)))
        return JSON_LEX_ERR_NUM_EXPECT_DIGIT; // expects a digit

    do {
        LEX_ADV_CHECK(lex, EOF);

    } while (is_digit(lex_char(lex)));

    return JSON_LEX_OK;
}

LEX_STIN JSON_LEX_ERR lexer_fraction(json_lexer_t* lex, bool* hasFrac) {
    char c = lex_char(lex);
    if (c != '.')
        return JSON_LEX_OK;

    *hasFrac = true;

    LEX_ADV_CHECK(lex, ERR_NUM_UNTERMINATED);

    return lexer_digits(lex);
}

LEX_STIN JSON_LEX_ERR lexer_exponent(json_lexer_t* lex, bool* hasExp) {
    char c = lex_char(lex);
    if (c != 'e' && c != 'E')
        return JSON_LEX_OK;

    *hasExp = true;

    LEX_ADV_CHECK(lex, ERR_NUM_UNTERMINATED);
    c = lex_char(lex);

    if (c == '+' || c == '-') {
        LEX_ADV_CHECK(lex, ERR_NUM_UNTERMINATED);
    }

    return lexer_digits(lex);
}

LEX_STIN JSON_LEX_ERR lexer_escape_seq(json_lexer_t* lex) {
    char c = lex_char(lex);

    if (is_escape_char(c)) {
        LEX_ADV_CHECK(lex, ERR_STR_UNTERMINATED); return JSON_LEX_OK;
    }

    if (c == 'u') {
        for (int i = 0; i < 4; ++i) {
            LEX_ADV_CHECK(lex, ERR_STR_UNTERMINATED);
            c = lex_char(lex);
            if (!is_hex_digit(c))
                return JSON_LEX_ERR_STR_INVALID_UNICODE;
        }

        LEX_ADV_CHECK(lex, ERR_STR_UNTERMINATED);
        return JSON_LEX_OK;
    }

    return JSON_LEX_ERR_STR_INVALID_ESCAPE;
}


LEX_STIN bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

LEX_STIN bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

LEX_STIN bool is_number_start(char c) {
    return is_digit(c) || c == '-';
}

LEX_STIN bool is_string_start(char c) {
    return c == '"';
}

LEX_STIN bool is_control_char(char c) {
    return c <= 0x1F;
}

LEX_STIN bool is_hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

LEX_STIN bool is_keyword_start(char c) {
    return c == 't' || c == 'f' || c == 'n';
}

LEX_STIN bool is_escape_char(char c) {
    return c == '"' || c == '\\' || c == '/' || c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't';
}



