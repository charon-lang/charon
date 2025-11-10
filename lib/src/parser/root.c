#include "charon/node.h"
#include "charon/parser.h"
#include "charon/token.h"
#include "parse.h"
#include "parser/parser.h"

void parse_root(charon_parser_t *parser) {
    parser_open_element(parser);

    while(!parser_is_eof(parser)) parse_tlc(parser);
    parser_consume(parser, CHARON_TOKEN_KIND_EOF);

    parser_close_element(parser, CHARON_NODE_KIND_ROOT);
}
