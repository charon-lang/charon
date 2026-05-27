#include "charon/lexer.h"

#include "charon/syntax/token.h"
#include "charon/syntax/trivia.h"
#include "common/text.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct charon_lexer {
    const uint8_t *data;
    size_t data_size;

    size_t cursor;
};

typedef struct {
    const char *string;
    charon_token_kind_t token_kind;
} keyword_t;

// Must be sorted by string for bsearch to work
static const keyword_t g_keyword_map[] = {
    { "as",       CHARON_TOKEN_KIND_KEYWORD_AS       },
    { "break",    CHARON_TOKEN_KIND_KEYWORD_BREAK    },
    { "continue", CHARON_TOKEN_KIND_KEYWORD_CONTINUE },
    { "default",  CHARON_TOKEN_KIND_KEYWORD_DEFAULT  },
    { "else",     CHARON_TOKEN_KIND_KEYWORD_ELSE     },
    { "enum",     CHARON_TOKEN_KIND_KEYWORD_ENUM     },
    { "extern",   CHARON_TOKEN_KIND_KEYWORD_EXTERN   },
    { "false",    CHARON_TOKEN_KIND_LITERAL_BOOL     },
    { "fn",       CHARON_TOKEN_KIND_KEYWORD_FUNCTION },
    { "for",      CHARON_TOKEN_KIND_KEYWORD_FOR      },
    { "if",       CHARON_TOKEN_KIND_KEYWORD_IF       },
    { "import",   CHARON_TOKEN_KIND_KEYWORD_IMPORT   },
    { "let",      CHARON_TOKEN_KIND_KEYWORD_LET      },
    { "module",   CHARON_TOKEN_KIND_KEYWORD_MODULE   },
    { "return",   CHARON_TOKEN_KIND_KEYWORD_RETURN   },
    { "sizeof",   CHARON_TOKEN_KIND_KEYWORD_SIZEOF   },
    { "struct",   CHARON_TOKEN_KIND_KEYWORD_STRUCT   },
    { "switch",   CHARON_TOKEN_KIND_KEYWORD_SWITCH   },
    { "true",     CHARON_TOKEN_KIND_LITERAL_BOOL     },
    { "type",     CHARON_TOKEN_KIND_KEYWORD_TYPE     },
    { "while",    CHARON_TOKEN_KIND_KEYWORD_WHILE    },
};

static int keyword_cmp(const void *a, const void *b) {
    return strcmp((const char *) a, ((const keyword_t *) b)->string);
}

static int read_utf8_codepoint(charon_lexer_t *lexer, uint32_t *out_codepoint) {
    uint8_t first_byte = lexer->data[lexer->cursor];

    if(first_byte <= 0x7F) {
        *out_codepoint = first_byte;
        return 1;
    }

    size_t codepoint_length;
    uint32_t codepoint;
    if((first_byte & 0xE0) == 0xC0) {
        codepoint_length = 2;
        codepoint = first_byte & 0x1F;
    } else if((first_byte & 0xF0) == 0xE0) {
        codepoint_length = 3;
        codepoint = first_byte & 0x0F;
    } else if((first_byte & 0xF8) == 0xF0) {
        codepoint_length = 4;
        codepoint = first_byte & 0x07;
    } else {
        return -1;
    }

    if(lexer->data_size - lexer->cursor < codepoint_length) return -1;

    for(size_t i = 1; i < codepoint_length; i++) {
        uint8_t b = lexer->data[lexer->cursor + i];
        if((b & 0xC0) != 0x80) return -1;
        codepoint = (codepoint << 6) | (b & 0x3F);
    }

    if(codepoint_length == 2 && codepoint < 0x0080) return -1;
    if(codepoint_length == 3 && codepoint < 0x0800) return -1;
    if(codepoint_length == 4 && codepoint < 0x10000) return -1;

    if(codepoint > 0x10FFFF) return -1;
    if(codepoint >= 0xD800 && codepoint <= 0xDFFF) return -1;

    *out_codepoint = codepoint;
    return codepoint_length;
}

charon_lexer_t *charon_lexer_new(const uint8_t *data, size_t data_size) {
    charon_lexer_t *lexer = malloc(sizeof(charon_lexer_t));
    lexer->data = data;
    lexer->data_size = data_size;
    lexer->cursor = 0;
    return lexer;
}

void charon_lexer_destroy(charon_lexer_t *lexer) {
    free(lexer);
}

charon_lexer_token_t charon_lexer_advance(charon_lexer_t *lexer) {
    enum lexer_state {
        STATE_INITIAL,

        STATE_WHITESPACE,
        STATE_COMMENT_LINE,
        STATE_COMMENT_MULTI,
        STATE_COMMENT_MULTI_END,

        STATE_IDENTIFIER,

        STATE_CHAR_FIRST,
        STATE_CHAR,
        STATE_STRING,
        STATE_STRING_RAW,
        STATE_STRING_RAW_LAST,

        STATE_NUMBER_PREFIX,
        STATE_NUMBER_DEC,
        STATE_NUMBER_HEX,
        STATE_NUMBER_OCT,
        STATE_NUMBER_BIN,

        STATE_PNCT_PIPE,
        STATE_PNCT_AMPERSAND,
        STATE_PNCT_CARET_LEFT,
        STATE_PNCT_CARET_RIGHT,
        STATE_PNCT_NOT,
        STATE_PNCT_PERCENTAGE,
        STATE_PNCT_STAR,
        STATE_PNCT_SLASH,
        STATE_PNCT_MINUS,
        STATE_PNCT_PLUS,
        STATE_PNCT_EQUAL,
        STATE_PNCT_COLON,
        STATE_PNCT_PERIOD,
        STATE_PNCT_PERIOD_DOUBLE,
    };

    charon_lexer_token_t token = {
        .start = lexer->cursor,
        .end = lexer->cursor,
        .is_trivia = false,
        .kind = { .token = CHARON_TOKEN_KIND_UNKNOWN },
    };

    if(lexer->cursor == lexer->data_size) {
        token.kind.token = CHARON_TOKEN_KIND_EOF;
        return token;
    }

    enum lexer_state state = STATE_INITIAL;
    while(lexer->cursor < lexer->data_size) {
        uint32_t codepoint;
        int codepoint_length = read_utf8_codepoint(lexer, &codepoint);
        if(codepoint_length == -1) {
            fprintf(stderr, "Encountered malformed utf8 codepoint");
            exit(1);
        }

        switch(state) {
            case STATE_INITIAL: {
                switch(codepoint) {
                    case 'a' ... 'z':
                    case 'A' ... 'Z':
                    case '_':         state = STATE_IDENTIFIER; break;

                    case '\'': state = STATE_CHAR; break;
                    case '\"': state = STATE_STRING; break;

                    case '0':         state = STATE_NUMBER_PREFIX; break;
                    case '1' ... '9': state = STATE_NUMBER_DEC; break;

                    case '\n':
                        token.is_trivia = true;
                        token.kind.trivia = CHARON_TRIVIA_KIND_NEWLINE;
                        goto terminate_consume;

                    case '\v':
                    case '\f':
                    case '\r':
                    case '\t':
                    case ' ':  state = STATE_WHITESPACE; break;

                    case '#': state = STATE_COMMENT_LINE; break;

                    case '@': token.kind.token = CHARON_TOKEN_KIND_PNCT_AT; goto terminate_consume;
                    case '^': token.kind.token = CHARON_TOKEN_KIND_PNCT_CARET; goto terminate_consume;
                    case '.': state = STATE_PNCT_PERIOD; break;
                    case ':': state = STATE_PNCT_COLON; break;
                    case ';': token.kind.token = CHARON_TOKEN_KIND_PNCT_SEMI_COLON; goto terminate_consume;
                    case '=': state = STATE_PNCT_EQUAL; break;
                    case '!': state = STATE_PNCT_NOT; break;
                    case '/': state = STATE_PNCT_SLASH; break;
                    case '%': state = STATE_PNCT_PERCENTAGE; break;
                    case '*': state = STATE_PNCT_STAR; break;
                    case '+': state = STATE_PNCT_PLUS; break;
                    case '-': state = STATE_PNCT_MINUS; break;
                    case '>': state = STATE_PNCT_CARET_RIGHT; break;
                    case '<': state = STATE_PNCT_CARET_LEFT; break;
                    case '(': token.kind.token = CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT; goto terminate_consume;
                    case ')': token.kind.token = CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT; goto terminate_consume;
                    case '{': token.kind.token = CHARON_TOKEN_KIND_PNCT_BRACE_LEFT; goto terminate_consume;
                    case '}': token.kind.token = CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT; goto terminate_consume;
                    case '[': token.kind.token = CHARON_TOKEN_KIND_PNCT_BRACKET_LEFT; goto terminate_consume;
                    case ']': token.kind.token = CHARON_TOKEN_KIND_PNCT_BRACKET_RIGHT; goto terminate_consume;
                    case ',': token.kind.token = CHARON_TOKEN_KIND_PNCT_COMMA; goto terminate_consume;
                    case '&': state = STATE_PNCT_AMPERSAND; break;
                    case '|': state = STATE_PNCT_PIPE; break;
                    default:  token.kind.token = CHARON_TOKEN_KIND_UNKNOWN; goto terminate_consume;
                }
            } break;

            case STATE_WHITESPACE: {
                switch(codepoint) {
                    case '\v':
                    case '\f':
                    case '\r':
                    case '\t':
                    case ' ':  break;
                    default:
                        token.is_trivia = true;
                        token.kind.trivia = CHARON_TRIVIA_KIND_WHITESPACE;
                        goto terminate;
                }
            } break;
            case STATE_COMMENT_LINE: {
                switch(codepoint) {
                    case '\n':
                        token.is_trivia = true;
                        token.kind.trivia = CHARON_TRIVIA_KIND_LINE_COMMENT;
                        goto terminate;
                    default: break;
                }
            } break;
            case STATE_COMMENT_MULTI: {
                switch(codepoint) {
                    case '*': state = STATE_COMMENT_MULTI_END; break;
                    default:  break;
                }
            } break;
            case STATE_COMMENT_MULTI_END: {
                switch(codepoint) {
                    case '/':
                        token.is_trivia = true;
                        token.kind.trivia = CHARON_TRIVIA_KIND_MULTI_COMMENT;
                        goto terminate_consume;
                    case '*': break;
                    default:  state = STATE_COMMENT_MULTI; break;
                }
            } break;

            case STATE_IDENTIFIER: {
                switch(codepoint) {
                    case 'a' ... 'z':
                    case 'A' ... 'Z':
                    case '0' ... '9':
                    case '_':         break;
                    default:          token.kind.token = CHARON_TOKEN_KIND_IDENTIFIER; goto terminate;
                }
            } break;

            case STATE_CHAR_FIRST: {
                switch(codepoint) {
                    case '\n': goto terminate;
                    case '\'': state = STATE_STRING_RAW; break;
                    default:   state = STATE_CHAR; break;
                }
            } break;
            case STATE_CHAR: {
                switch(codepoint) {
                    case '\n': goto terminate;
                    case '\'': token.kind.token = CHARON_TOKEN_KIND_LITERAL_CHAR; goto terminate_consume;
                    default:   break;
                }
            } break;
            case STATE_STRING: {
                switch(codepoint) {
                    case '\n': goto terminate;
                    case '\"': token.kind.token = CHARON_TOKEN_KIND_LITERAL_STRING; goto terminate_consume;
                    default:   break;
                }
            } break;
            case STATE_STRING_RAW: {
                switch(codepoint) {
                    case '\'': state = STATE_STRING_RAW_LAST; break;
                    default:   break;
                }
            } break;
            case STATE_STRING_RAW_LAST: {
                switch(codepoint) {
                    case '\'': token.kind.token = CHARON_TOKEN_KIND_LITERAL_STRING_RAW; goto terminate_consume;
                    default:   state = STATE_STRING_RAW; break;
                }
            } break;

            case STATE_NUMBER_PREFIX: {
                switch(codepoint) {
                    case '0' ... '9': state = STATE_NUMBER_DEC; break;
                    case 'x':         state = STATE_NUMBER_HEX; break;
                    case 'o':         state = STATE_NUMBER_OCT; break;
                    case 'b':         state = STATE_NUMBER_BIN; break;
                    default:          token.kind.token = CHARON_TOKEN_KIND_LITERAL_NUMBER_DEC; goto terminate;
                }
            } break;
            case STATE_NUMBER_DEC: {
                switch(codepoint) {
                    case '0' ... '9': break;
                    default:          token.kind.token = CHARON_TOKEN_KIND_LITERAL_NUMBER_DEC; goto terminate;
                }
            } break;
            case STATE_NUMBER_HEX: {
                switch(codepoint) {
                    case '0' ... '9':
                    case 'a' ... 'f': break;
                    case 'A' ... 'F': break;
                    default:          token.kind.token = CHARON_TOKEN_KIND_LITERAL_NUMBER_HEX; goto terminate;
                }
            } break;
            case STATE_NUMBER_OCT: {
                switch(codepoint) {
                    case '0' ... '7': break;
                    default:          token.kind.token = CHARON_TOKEN_KIND_LITERAL_NUMBER_OCT; goto terminate;
                }
            } break;
            case STATE_NUMBER_BIN: {
                switch(codepoint) {
                    case '0' ... '1': break;
                    default:          token.kind.token = CHARON_TOKEN_KIND_LITERAL_NUMBER_BIN; goto terminate;
                }
            } break;

            case STATE_PNCT_PIPE: {
                switch(codepoint) {
                    case '|': token.kind.token = CHARON_TOKEN_KIND_PNCT_LOGICAL_OR; goto terminate_consume;
                    default:  token.kind.token = CHARON_TOKEN_KIND_PNCT_PIPE; goto terminate;
                }
            } break;

            case STATE_PNCT_AMPERSAND: {
                switch(codepoint) {
                    case '&': token.kind.token = CHARON_TOKEN_KIND_PNCT_LOGICAL_AND; goto terminate_consume;
                    default:  token.kind.token = CHARON_TOKEN_KIND_PNCT_AMPERSAND; goto terminate;
                }
            } break;
            case STATE_PNCT_CARET_LEFT: {
                switch(codepoint) {
                    case '=': token.kind.token = CHARON_TOKEN_KIND_PNCT_LESS_EQUAL; goto terminate_consume;
                    case '<': token.kind.token = CHARON_TOKEN_KIND_PNCT_SHIFT_LEFT; goto terminate_consume;
                    default:  token.kind.token = CHARON_TOKEN_KIND_PNCT_CARET_RIGHT; goto terminate;
                }
            } break;
            case STATE_PNCT_CARET_RIGHT: {
                switch(codepoint) {
                    case '=': token.kind.token = CHARON_TOKEN_KIND_PNCT_GREATER_EQUAL; goto terminate_consume;
                    case '>': token.kind.token = CHARON_TOKEN_KIND_PNCT_SHIFT_RIGHT; goto terminate_consume;
                    default:  token.kind.token = CHARON_TOKEN_KIND_PNCT_CARET_LEFT; goto terminate;
                }
            } break;
            case STATE_PNCT_NOT: {
                switch(codepoint) {
                    case '=': token.kind.token = CHARON_TOKEN_KIND_PNCT_NOT_EQUAL; goto terminate_consume;
                    default:  token.kind.token = CHARON_TOKEN_KIND_PNCT_NOT; goto terminate;
                }
            } break;
            case STATE_PNCT_PERCENTAGE: {
                switch(codepoint) {
                    case '=': token.kind.token = CHARON_TOKEN_KIND_PNCT_PERCENTAGE_EQUAL; goto terminate_consume;
                    default:  token.kind.token = CHARON_TOKEN_KIND_PNCT_PERCENTAGE; goto terminate;
                }
            } break;
            case STATE_PNCT_STAR: {
                switch(codepoint) {
                    case '=': token.kind.token = CHARON_TOKEN_KIND_PNCT_STAR_EQUAL; goto terminate_consume;
                    default:  token.kind.token = CHARON_TOKEN_KIND_PNCT_STAR; goto terminate;
                }
            } break;
            case STATE_PNCT_SLASH: {
                switch(codepoint) {
                    case '/': state = STATE_COMMENT_LINE; break;
                    case '*': state = STATE_COMMENT_MULTI; break;
                    case '=': token.kind.token = CHARON_TOKEN_KIND_PNCT_SLASH_EQUAL; goto terminate_consume;
                    default:  token.kind.token = CHARON_TOKEN_KIND_PNCT_SLASH; goto terminate;
                }
            } break;
            case STATE_PNCT_MINUS: {
                switch(codepoint) {
                    case '>': token.kind.token = CHARON_TOKEN_KIND_PNCT_ARROW; goto terminate_consume;
                    case '=': token.kind.token = CHARON_TOKEN_KIND_PNCT_MINUS_EQUAL; goto terminate_consume;
                    default:  token.kind.token = CHARON_TOKEN_KIND_PNCT_MINUS; goto terminate;
                }
            } break;
            case STATE_PNCT_PLUS: {
                switch(codepoint) {
                    case '=': token.kind.token = CHARON_TOKEN_KIND_PNCT_PLUS_EQUAL; goto terminate_consume;
                    default:  token.kind.token = CHARON_TOKEN_KIND_PNCT_PLUS; goto terminate;
                }
            } break;
            case STATE_PNCT_EQUAL: {
                switch(codepoint) {
                    case '>': token.kind.token = CHARON_TOKEN_KIND_PNCT_THICK_ARROW; goto terminate_consume;
                    case '=': token.kind.token = CHARON_TOKEN_KIND_PNCT_DOUBLE_EQUAL; goto terminate_consume;
                    default:  token.kind.token = CHARON_TOKEN_KIND_PNCT_EQUAL; goto terminate;
                }
            } break;
            case STATE_PNCT_COLON: {
                switch(codepoint) {
                    case ':': token.kind.token = CHARON_TOKEN_KIND_PNCT_DOUBLE_COLON; goto terminate_consume;
                    default:  token.kind.token = CHARON_TOKEN_KIND_PNCT_COLON; goto terminate;
                }
            } break;
            case STATE_PNCT_PERIOD: {
                switch(codepoint) {
                    case '.': state = STATE_PNCT_PERIOD_DOUBLE; break;
                    default:  token.kind.token = CHARON_TOKEN_KIND_PNCT_PERIOD; goto terminate;
                }
            } break;
            case STATE_PNCT_PERIOD_DOUBLE: {
                switch(codepoint) {
                    case '.': token.kind.token = CHARON_TOKEN_KIND_PNCT_TRIPLE_PERIOD; goto terminate_consume;
                    default:  token.kind.token = CHARON_TOKEN_KIND_PNCT_DOUBLE_PERIOD; goto terminate;
                }
            } break;

            terminate_consume: {
                lexer->cursor += codepoint_length;
                goto terminate;
            }
        }

        lexer->cursor += codepoint_length;
    }

terminate:
    token.end = lexer->cursor;

    if(!token.is_trivia && token.kind.token == CHARON_TOKEN_KIND_IDENTIFIER) {
        size_t token_length = token.end - token.start;
        char *lookup = malloc(token_length + 1);
        memcpy(lookup, &lexer->data[token.start], token_length);
        lookup[token_length] = '\0';

        keyword_t *keyword = bsearch(lookup, g_keyword_map, sizeof(g_keyword_map) / sizeof(keyword_t), sizeof(keyword_t), keyword_cmp);

        free(lookup);

        if(keyword != nullptr) token.kind.token = keyword->token_kind;
    }

    return token;
}

text_t *lexer_extract(charon_lexer_t *lexer, charon_lexer_token_t token) {
    size_t text_size = token.end - token.start;
    text_t *text = malloc(sizeof(text_t) + text_size);
    text->size = text_size;
    memcpy(&text->data, &lexer->data[token.start], text_size);
    return text;
}
