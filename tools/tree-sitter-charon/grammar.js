/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

module.exports = grammar({
    name: "charon",

    word: $ => $.identifier,

    extras: $ => [/\s/, $.comment],

    externals: $ => [
        $.multiline_comment
    ],

    rules: {
        source_file: $ => repeat($._tlc),

        comment: $ => choice(
            seq("//", /(\\+(.|\r?\n)|[^\\\n])*/),
            $.multiline_comment
        ),

        // stmt
        _tlc: $ => seq(
            repeat($.attribute),
            choice(
                $.tlc_module,
                $.tlc_function,
                $.tlc_extern,
                $.tlc_declaration,
                $.tlc_type,
                $.tlc_enum
            )
        ),

        tlc_module: $ => seq("module", field("name", $.identifier), "{", field("body", repeat($._tlc)), "}"),
        tlc_function: $ => seq("fn", field("name", $.identifier), field("type", $.function_type), field("body", $._stmt)),
        tlc_extern: $ => seq("extern", "fn", field("name", $.identifier), field("type", $.function_type), ";"),
        tlc_declaration: $ => seq("let", field("name", $.identifier), ":", field("type", $.type), ";"),
        tlc_type: $ => seq("type", field("name", $.identifier), field("type", $.type)),
        tlc_enum: $ => seq("enum", field("name", $.identifier), "{", optional(seq(field("member", $.identifier), repeat(seq(",", field("member",$.identifier))))), "}"),

        // stmt
        _stmt: $ => seq(
            repeat($.attribute),
            choice(
                ";",
                $.stmt_if,
                $.stmt_while,
                $.stmt_for,
                $.stmt_block,
                $.stmt_simple
            )
        ),
        stmt_simple: $ => seq(
            choice(
                $.stmt_return,
                $.stmt_decl,
                "continue",
                "break",
                $._expr
            ),
            ";"
        ),

        // TODO: not sure whether this is left or right associative
        stmt_if: $ => prec.left(seq("if", "(", $._expr, ")", $._stmt, optional(seq("else", $._stmt)))),
        stmt_while: $ => seq("while", optional(seq("(", $._expr, ")")), $._stmt),
        stmt_for: $ => seq("for", "(", optional($.stmt_decl), ";", optional($._expr), ";", optional($._expr), ")", $._stmt),
        stmt_block: $ => seq("{", repeat($._stmt), "}"),

        stmt_return: $ => seq("return", field("value", optional($._expr))),
        stmt_decl: $ => seq("let", field("name", $.identifier), field("type", optional(seq(":", $.type))), field("value", optional(seq("=", $._expr)))),

        // expr
        _expr: $ => choice(prec(1, $.expr_binary), prec(2, $.expr_unary_pre), prec(2, $.expr_unary_post), prec(3, $._expr_primary)),

        expr_binary: $ => choice(
            prec.right(1, seq($._expr, choice("=", "+=", "-=", "*=", "/=", "%="), $._expr)),
            prec.left(2, seq($._expr, "||", $._expr)),
            prec.left(3, seq($._expr, "&&", $._expr)),
            prec.left(4, seq($._expr, "|", $._expr)),
            prec.left(5, seq($._expr, "^", $._expr)),
            prec.left(6, seq($._expr, "&", $._expr)),
            prec.left(7, seq($._expr, choice("==", "!="), $._expr)),
            prec.left(8, seq($._expr, choice(">", ">=", "<", "<="), $._expr)),
            prec.left(9, seq($._expr, choice("<<", ">>"), $._expr)),
            prec.left(10, seq($._expr, choice("+", "-"), $._expr)),
            prec.left(11, seq($._expr, choice("*", "/", "%"), $._expr)),
        ),

        expr_unary_pre: $ => prec.right(seq(
            field("operation", choice("*", "-", "!", "&")),
            field("value", $._expr)
        )),

        expr_unary_post: $ => prec.left(seq(
            field("value", $._expr),
            field("operation", choice(
                seq("as", $.type),
                seq("[", $._expr, "]"),
                $.expr_unary_call,
                $.expr_unary_subscript_const,
                $.expr_unary_subscript_arrow
            ))
        )),

        expr_unary_call: $ => seq("(", optional(seq($._expr, repeat(seq(",", $._expr)))), ")"),
        expr_unary_subscript_const: $ => seq(".", choice($.number_dec, $.identifier)),
        expr_unary_subscript_arrow: $ => seq("->", $.identifier),

        _expr_primary: $ => choice(
            $.expr_tuple,
            $.expr_identifier,
            $.literal
        ),

        expr_tuple: $ => prec.left(seq("(", $._expr, repeat(seq(",", $._expr)), ")")),
        expr_identifier: $ => seq(repeat(seq(field("selector", $.identifier), "::")), field("name", $.identifier)),

        // util
        type: $ => seq(
            repeat($.attribute),
            choice(
                $.type_struct,
                $.type_tuple,
                $.type_array,
                seq("*", $.type),
                seq("fn", $.function_type),
                $.type_primary
            )
        ),
        type_struct: $ => seq("struct", "{", repeat(seq(field("member_name", $.identifier), ":", field("member_type", $.type), ";")), "}"),
        type_tuple: $ => seq("(", $.type, repeat(seq(",", $.type)), ")"),
        type_array: $ => seq("[", field("size", $.number), "]", field("type", $.type)),
        type_primary: $ => seq(repeat(seq(field("selector", $.identifier), "::")), field("name", $.identifier)),

        function_argument: $ => choice(seq(field("name", $.identifier), ":", field("type", $.type)), "..."),
        function_type: $ => seq("(", field("arguments", optional(seq($.function_argument, repeat(seq(",", $.function_argument))))), ")", field("return_type", optional(seq(":", $.type)))),
        attribute: $ => prec.left(seq("@", field("name", $.identifier), optional(seq("(", optional(seq($.literal, repeat(seq(",", $.literal)))), ")")))),

        // lexer
        identifier: $ => /[_a-zA-Z][_a-zA-Z0-9]*/,

        number_hex: $ => /0x[a-fA-F\d]+/,
        number_bin: $ => /0b[01]+/,
        number_oct: $ => /0o[0-7]+/,
        number_dec: $ => /\d+/,
        number: $ => choice($.number_dec, $.number_hex, $.number_bin, $.number_oct),

        string_raw: $ => /''(?:.|\n)*?''/,
        string: $ => seq(
            "\"",
            repeat(choice(
                alias(token.immediate(/[^\\"\n]+/), $.string_content),
                $.escape_sequence
            )),
            "\""
        ),
        char: $ => seq(
            "'",
            repeat(choice(
                alias(token.immediate(/[^\n']/), $.char_content),
                $.escape_sequence
            )),
            "'"
        ),
        escape_sequence: $ => choice(
            prec(2, alias(token(seq("\\", choice(/[\\'"?abfnrtv]/, /\d{1,3}/))), "valid")),
            prec(1, alias(token(seq("\\", /./)), "invalid"))
        ),

        bool: $ => choice("true", "false"),

        literal: $ => choice($.number, $.string, $.string_raw, $.char, $.bool)
    }
});
