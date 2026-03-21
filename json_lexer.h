#ifndef JSON_LEXER_H
#define JSON_LEXER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define JSON_TOKEN_STR(x)                                        \
	(((x) < 0 || (x) >= JSON_TOKEN__FILLER_LAST) ? "UNKNOWN" \
						     : JSON__TOKEN_STRINGS[x])

#define JSON_LEX_ERR_STR(x)                                        \
	(((x) < 0 || (x) >= JSON_LEX__FILLER_LAST) ? "ERR_UNKNOWN" \
						   : JSON__LEX_ERR_STRINGS[x])

#define JSON_TOKEN_TYPES(X) \
	X(LBRACE)           \
	X(RBRACE)           \
	X(LBRACKET)         \
	X(RBRACKET)         \
	X(COMMA)            \
	X(COLON)            \
	X(INT)              \
	X(DOUBLE)           \
	X(STRING)           \
	X(TRUE)             \
	X(FALSE)            \
	X(NULL)             \
	X(INVALID)          \
	X(_FILLER_LAST)	 // to get number of elements

typedef enum {
#define X(name) JSON_TOKEN_##name,
	JSON_TOKEN_TYPES(X)
#undef X
} JSON_TOKEN_TYPE;

extern const char* JSON__TOKEN_STRINGS[];

#define JSON_LEX_ERR_TYPES(X)      \
	X(OK = 0)                  \
	X(EOF)                     \
	X(ERR_LINE_OVERFLOW)       \
	X(ERR_COL_OVERFLOW)        \
	X(ERR_STR_CONTROL_CHAR)    \
	X(ERR_STR_INVALID_UNICODE) \
	X(ERR_STR_INVALID_ESCAPE)  \
	X(ERR_STR_UNTERMINATED)    \
	X(ERR_STR_TOO_LONG)        \
	X(ERR_NUM_EXPECT_DIGIT)    \
	X(ERR_NUM_LEADING_ZERO)    \
	X(ERR_NUM_UNTERMINATED)    \
	X(ERR_NUM_RANGE)           \
	X(ERR_NUM_INVALID)         \
	X(ERR_INVALID_KEYWORD)     \
	X(ERR_UNEXPECTED_SYMBOL)   \
	X(_FILLER_LAST)

typedef enum {
#define X(name) JSON_LEX_##name,
	JSON_LEX_ERR_TYPES(X)
#undef X
} JSON_LEX_ERR;

extern const char* JSON__LEX_ERR_STRINGS[];

// 32bytes
typedef struct {
	union {
		int64_t intVal;
		double dblVal;
	} number;

	const char* start;
	uint32_t strLen;

	uint32_t line, col;

	JSON_TOKEN_TYPE type;

} json_token_t;

// 80bytes
typedef struct {
	const char* input_start;
	const char* input_end;

	const char* parse_point;

	size_t line, col;

	json_token_t token;

	JSON_LEX_ERR last_status;

} json_lexer_t;

void json_lexer_init(json_lexer_t* lex, const char* input_stream,
		     const char* input_end);

JSON_LEX_ERR json_lexer_next_token(json_lexer_t* lex);

#endif
