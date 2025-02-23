#include "tree_sitter/parser.h"
#include "tree_sitter/alloc.h"
#include "tree_sitter/array.h"

enum TokenType {
    MULTILINE_COMMENT
};

void *tree_sitter_charon_external_scanner_create(void) {
    return NULL;
}

void tree_sitter_charon_external_scanner_destroy(void *payload) {}

unsigned tree_sitter_charon_external_scanner_serialize(void *payload, char *buffer) {
    return 0;
}

void tree_sitter_charon_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {}

bool tree_sitter_charon_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
    enum { NONE, ISTAR, IN, OSTAR, OUT } state = NONE;
    lexer->result_symbol = MULTILINE_COMMENT;
    while(!lexer->eof(lexer)) {
        switch(state) {
            case NONE:
                if(lexer->lookahead == '/') { state = ISTAR; break; }
                return false;
            case ISTAR:
                if(lexer->lookahead == '*') { state = IN; break; }
                return false;
            case IN:
                if(lexer->lookahead == '*') state = OSTAR;
                break;
            case OSTAR:
                if(lexer->lookahead == '/') { state = OUT; break; }
                state = IN;
                break;
            case OUT: return true;
        }
        lexer->advance(lexer, false);
    }
    return state >= IN;
}