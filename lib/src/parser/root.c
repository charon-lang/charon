#include "charon/element.h"
#include "charon/node.h"
#include "charon/parser.h"
#include "parse.h"
#include "parser/collector.h"
#include "parser/parser.h"

charon_element_inner_t *parse_root(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    while(!parser_is_eof(parser)) collector_push(&collector, parse_tlc(parser));
    return parser_build(parser, CHARON_NODE_KIND_ROOT, &collector);
}
