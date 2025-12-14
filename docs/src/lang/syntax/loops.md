# Loops

## While

```ebnf
WhileStmt =
    'while' , ( '(' , Expr , ')' ) ? , Stmt ;
```

The while loop will execute its body until a boolean predicate evaluates to false. The predicate can also be left out for an infinite loop.

## For

```ebnf
ForStmt =
    'for' , '(' , ( StmtDecl | ';' ) , Expr ? , ';' , Expr ? , ')' , Stmt ;
```

The for loop takes a declaration, a boolean predicate, and an expression. The loop will execute its body until the predicate evaluates to false.
The expression will run after every iteration.
