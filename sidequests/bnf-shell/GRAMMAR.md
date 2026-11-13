# Read the shell grammar

These diagrams describe [Grammar.cf](Grammar.cf), not the full POSIX shell.
Boxes are categories (nonterminals), tokens, or constructor alternatives.
An edge means “contains this part,” **not execution order**. An edge back to
its own category shows recursion. Quoted text in the rules is literal syntax.

## 1. Input and job overview

```mermaid
flowchart TD
    I["Input — sole public entry point"]
    I --> A["Assign: Word = Word"]
    I --> TA["ThenAssign: Word = Word ; Input"]
    TA -->|"remaining input — recursive"| I
    I --> LF["LastFG: Pipeline"]
    I --> LB["LastBG: Pipeline &"]
    I --> TF["ThenFG: Pipeline ; Input"]
    I --> TB["ThenBG: Pipeline & Input"]
    TF -->|"remaining input — recursive"| I
    TB -->|"remaining input — recursive"| I
    LF --> P[Pipeline]
    LB --> P
    TF --> P
    TB --> P
    I --> IF["If: if Pipeline ; then Pipeline ; fi"]
    IF -->|"condition — first pipeline"| P
    IF -->|"body — second pipeline"| P
    A -->|"name and literal value"| W[Word]
    TA -->|"name and literal value"| W
```

`ThenFG` and `ThenBG` store the first pipeline and the rest of the input.
`ThenAssign` does the same for an assignment. Thus `x=5 ; echo $x` is one
`ThenAssign` node with another `Input` inside it. A conditional has exactly
two pipelines, not two complete `Input` trees. This keeps its body small.
The runtime chooses whether to evaluate the second pipeline.

## 2. Commands, arguments, and inner pipelines

```mermaid
flowchart TD
    P[Pipeline] -->|Single| C[Command]
    P -->|"Pipe — first stage"| C
    P -->|"Pipe — remaining stages, recursive"| P
    C -->|"Cmd — command name"| W[Word]
    C -->|"Cmd — argument list"| LA["ListArg / [Arg]"]
    C -->|"Cmd — optional file I/O"| R[OptRedir]
    LA -->|"empty list"| E["empty"]
    LA -->|"one list item"| A[Arg]
    LA -->|"remaining list — recursive"| LA
    A -->|Literal| W
    A -->|"Variable — dollar sign then Word"| W
    A -->|"Subcommand — between backquotes"| IP[InnerPipeline]
    IP -->|InnerSingle| IC[InnerCommand]
    IP -->|"InnerPipe — first stage"| IC
    IP -->|"InnerPipe — remaining stages, recursive"| IP
    IC -->|"InnerCmd — command name"| W
    IC -->|"InnerCmd — argument list"| LIA["ListInnerArg / [InnerArg]"]
    IC -->|"InnerCmd — optional file I/O"| R
    LIA -->|"empty list"| E
    LIA -->|"one list item"| IA[InnerArg]
    LIA -->|"remaining list — recursive"| LIA
    IA -->|InnerLiteral| W
    IA -->|"InnerVariable — dollar sign then Word"| W
```

`Pipe` is right-recursive: `echo hi | cat | wc` holds the first `Command`,
then a `Pipeline` that contains `cat | wc`. `Single` ends this chain.
`InnerPipe` and `InnerSingle` use the same shape inside backquotes.

Both list categories permit zero or more items. BNFC's `separator Arg ""`
and `separator InnerArg ""` rules generate empty and cons list constructors;
no punctuation separates arguments. The lexer skips whitespace. The diagrams
show the logical list structure; BNFC can reverse an intermediate list during
parsing to preserve source argument order in the final AST.

An outer `Arg` can contain an `InnerPipeline`. An `InnerArg` **cannot** contain
a subcommand. There is no edge from `InnerArg` back to `InnerPipeline`. This
is how the grammar prevents nested backquotes without a lexer mode.

## 3. Per-command optional redirection

```mermaid
flowchart LR
    R[OptRedir] --> N["NoRedir: empty"]
    R --> IN["InRedir: < Word"]
    R --> OUT["OutRedir: > Word"]
    R --> IO["InOutRedir: < Word > Word"]
    R --> OI["OutInRedir: > Word < Word"]
    IN -->|"input file"| W[Word]
    OUT -->|"output file"| W
    IO -->|"input then output file"| W
    OI -->|"output then input file"| W
```

`OptRedir` is attached to `Command` and `InnerCommand`, not `Pipeline`.
For example, `cat < in.txt | wc > out.txt` gives each command its own
redirection node. `NoRedir` is a real AST constructor with no source token.
There is no repeated input, repeated output, append, or stderr rule.

## Token and runtime distinctions

`Word` is one or more letters, digits, or `_ . / - :`. Examples: `5`, `-l`,
`./input-file_1.txt`. `if`, `then`, and `fi` are reserved terminals, not words.
`$`, backquotes, `|`, `;`, `&`, `<`, `>`, and `=` are syntax tokens.
There are no quoting or escaping rules.

`echo`, `hello`, `cd`, `exit`, and `export` are all ordinary command-name
`Word` tokens. The grammar does not decide which commands use a thread,
a process, or the main thread. The runtime makes that decision after parsing.
Likewise, `Word` permits `5bad` as an assignment name; the runtime rejects it.

## Concrete AST, checked with the generated parser

Input:

```text
x=5 ; echo $x | wc -c
```

Generated abstract syntax (line-wrapped here only):

```text
(ThenAssign "x" "5"
  (LastFG
    (Pipe (Cmd "echo" [(Variable "x")] NoRedir)
      (Single (Cmd "wc" [(Literal "-c")] NoRedir)))))
```

Read it from the outside in:

1. `ThenAssign` stores the name `x`, literal value `5`, and remaining input.
2. `LastFG` says that the remaining pipeline runs in the foreground.
3. `Pipe` stores the first command and the remaining single-stage pipeline.
4. `Variable` records a lookup request; parsing does not replace `$x` with `5`.
5. Both commands have `NoRedir`. At runtime a system pipe connects them.

Reproduce this AST without executing commands:

```sh
printf 'x=5 ; echo $x | wc -c\n' | ./build/TestGrammar
```

The generated Bison file has an additional private untyped `ShellStart`
wrapper from `prepare_parser.py`. It only transfers AST ownership for cleanup;
it is not a `Grammar.cf` category, public entry point, or language feature.
