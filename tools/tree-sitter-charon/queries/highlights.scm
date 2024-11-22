[
    "return"
    "if"
    "else"
    "while"
    "fn"
    "let"
    "as"
    "extern"
    "module"
    "type"
    "struct"
    "continue"
    "break"
    "enum"
    "for"
] @keyword

(bool) @languageDefinedValue
[(number_hex) (number_dec) (number_bin) (number_oct)] @number

(char ["'" (char_content)] @number)
(char ([(char_content) (escape_sequence)] [(char_content) (escape_sequence)]* @invalid))

(string_raw) @string
(string ["\"" (string_content)] @string)

(escape_sequence "valid" @string.escape)
(escape_sequence "invalid" @invalid)

(type_primary
    selector: (identifier) @namespace)

(type_primary
    name: (identifier) @type)

(type_struct
    member_name: (identifier) @property)

(function_argument
    name: (identifier) @parameter)


(tlc_enum
    name: (identifier) @namespace
    member: (identifier) @enumMember)

(tlc_module
    name: (identifier) @namespace)

(tlc_type
    name: (identifier) @namespace)

(tlc_function
    name: (identifier) @function)

(tlc_extern
    name: (identifier) @function)


(stmt_decl
    name: (identifier) @variable)


(expr_identifier
    name: (identifier) @variable)

(expr_identifier
    selector: (identifier) @namespace)


(expr_unary_post
    value: (expr_identifier
        name: (identifier) @function)
    operation: (expr_unary_call))

(expr_unary_subscript_const (number_dec) @number)
(expr_unary_subscript_const (identifier) @variable)
(expr_unary_subscript_arrow (identifier) @variable)

(comment) @comment
(multiline_comment) @comment