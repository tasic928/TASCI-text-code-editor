#ifndef LSP_AUTOCOMPLETE_H
#define LSP_AUTOCOMPLETE_H

#include <string.h>

static const char *autocomplete_langs[] = {
    "C++",
    "Golang",
    "Java",
    "JavaScript",
    "Python",
    "R",
    "C",
    "Csharp",
    "Julia",
    "Perl",
    "Matlab",
    "Kotlin",
    "PHP",
    "Ruby",
    "Rust",
    "TypeScript",
    "Lua",
    "SAS",
    "Fortran",
    "Lisp",
    "Scala",
    "Assembly",
    "ActionScript",
    "Clojure",
    "CoffeeScript",
    "Dart",
    "COBOL",
    "Elixir",
    "Groovy",
    "Erlang",
    "Haskell",
    "Pascal",
    "Swift",
    "Scheme",
    "Racket",
    "OCaml",
    "Elm",
    "Haxe",
    "Crystal",
    "Fsharp",
    "Tcl",
    "VB.NET",
    "Objective_C",
    "Ada",
    "Vala",
    "PySpark",
    "SQL",
    "VB6",
    "VBA",
    "VBScript",
    "PowerShell",
    "Bash",
    "Delphi",
    "Zig",
    "Carbon",
    "Nim",
    "Grain",
    "Gleam",
    "Wren",
    "Janet",
    "Oberon+",
    "Raku",
    NULL
};

static inline int autocomplete_lang_enabled(const char *name) {
    if (!name) return 0;
    for (int i = 0; autocomplete_langs[i]; i++) {
        if (strcmp(autocomplete_langs[i], name) == 0) return 1;
    }
    return 0;
}

#endif
