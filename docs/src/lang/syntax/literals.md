# Literals

## Strings

String literals in Charon can be defined using double quotes. String literals allow for escape sequences which are denoted by a `\`.

```none
"hello world\n"
```

Raw string literals can be defined using two single quotes (`''`). Raw string literals will not interpolate escape sequences and can occupy multiple lines.

```none
''
hello
world
''
```

## Integers

There are four different formats for defining integer literals:

- Decimal (`500`)
- Hexadecimal (`0x500`)
- Binary (`0b101101`)
- Octal (`0o500`)

## Character

Character literals are defined using a single pair of single quotes. Character literals support escape sequences similar to strings.

```none
"a"
```

## Boolean

Boolean literals are defined using the keywords `true` and `false`.

## Null

Null literals are defined using the keyword `null`.
