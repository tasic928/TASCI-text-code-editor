#ifndef SYNTAX_HIGHLIGHTING_H
#define SYNTAX_HIGHLIGHTING_H

#include <string.h>
#include <ctype.h>

typedef struct {
    const char *name;
    const char **exts;     /* file extensions, without dot, NULL-terminated */
    const char **keywords; /* keywords, NULL-terminated */
    const char *line_comment;        /* e.g. //, #, -- */
    const char *block_comment_start; /* e.g. "/ *" (slash-asterisk) */
    const char *block_comment_end;   /* e.g. "* /" (asterisk-slash) */
    const char *string_delims;       /* characters that start/end strings, e.g. "\"'`" */
    int flags;
} SyntaxLang;

enum {
    SH_FLAG_KW_CASE_INSENSITIVE = 1 << 0,
};

static inline int sh_word_eq(const char *w, int len, const char *kw) {
    return (int)strlen(kw) == len && strncmp(w, kw, (size_t)len) == 0;
}

static inline int sh_word_eq_ci(const char *w, int len, const char *kw) {
    if ((int)strlen(kw) != len) return 0;
    for (int i = 0; i < len; i++) {
        char a = (char)tolower((unsigned char)w[i]);
        char b = (char)tolower((unsigned char)kw[i]);
        if (a != b) return 0;
    }
    return 1;
}

static inline int sh_is_keyword(const SyntaxLang *lang, const char *w, int len) {
    if (!lang || !w || len <= 0) return 0;
    int ci = (lang->flags & SH_FLAG_KW_CASE_INSENSITIVE) != 0;
    for (int i = 0; lang->keywords[i]; i++) {
        if (ci) {
            if (sh_word_eq_ci(w, len, lang->keywords[i])) return 1;
        } else {
            if (sh_word_eq(w, len, lang->keywords[i])) return 1;
        }
    }
    return 0;
}

static inline const char *sh_ext_from_path(const char *path) {
    if (!path) return NULL;
    const char *dot = strrchr(path, '.');
    if (!dot || dot == path || !dot[1]) return NULL;
    return dot + 1;
}

static inline int sh_ext_matches(const char *ext, const char **exts) {
    if (!ext || !exts) return 0;
    for (int i = 0; exts[i]; i++) {
        if (strcmp(ext, exts[i]) == 0) return 1;
    }
    return 0;
}

static inline int sh_contains_ci(const char *hay, const char *needle) {
    if (!hay || !needle || !needle[0]) return 0;
    size_t nlen = strlen(needle);
    for (const char *p = hay; *p; p++) {
        size_t i = 0;
        while (i < nlen && p[i]) {
            char a = (char)tolower((unsigned char)p[i]);
            char b = (char)tolower((unsigned char)needle[i]);
            if (a != b) break;
            i++;
        }
        if (i == nlen) return 1;
    }
    return 0;
}

/* ---- Keywords ---- */
static const char *kw_c[] = {
    "auto","break","case","char","const","continue","default","do",
    "double","else","enum","extern","float","for","goto","if","inline",
    "int","long","register","restrict","return","short","signed","sizeof",
    "static","struct","switch","typedef","union","unsigned","void","volatile","while",
    NULL
};

static const char *kw_cpp[] = {
    "alignas","alignof","asm","auto","bool","break","case","catch","char",
    "class","const","constexpr","continue","decltype","default","delete","do",
    "double","else","enum","explicit","export","extern","false","float","for",
    "friend","goto","if","inline","int","long","mutable","namespace","new",
    "noexcept","nullptr","operator","private","protected","public","register",
    "reinterpret_cast","return","short","signed","sizeof","static","struct",
    "switch","template","this","throw","true","try","typedef","typeid",
    "typename","union","unsigned","using","virtual","void","volatile","while",
    NULL
};

static const char *kw_d[] = {
    "alias","align","asm","assert","auto","body","bool","break","byte","case","cast",
    "catch","cdouble","cent","cfloat","char","class","const","continue","creal",
    "dchar","debug","default","delegate","delete","deprecated","do","double","else",
    "enum","export","extern","false","final","finally","float","for","foreach",
    "foreach_reverse","function","goto","if","immutable","import","in","inout",
    "interface","invariant","is","lazy","long","macro","mixin","module","new",
    "nothrow","null","out","override","package","pragma","private","protected",
    "public","pure","real","ref","return","scope","shared","short","static",
    "struct","super","switch","synchronized","template","this","throw","true",
    "try","typedef","typeid","typeof","ubyte","ucent","uint","ulong","union",
    "unittest","ushort","version","void","volatile","wchar","while","with",
    NULL
};

static const char *kw_go[] = {
    "break","case","chan","const","continue","default","defer","else","fallthrough",
    "for","func","go","goto","if","import","interface","map","package","range",
    "return","select","struct","switch","type","var", NULL
};

static const char *kw_java[] = {
    "abstract","assert","boolean","break","byte","case","catch","char","class","const",
    "continue","default","do","double","else","enum","extends","final","finally","float",
    "for","goto","if","implements","import","instanceof","int","interface","long",
    "native","new","package","private","protected","public","return","short","static",
    "strictfp","super","switch","synchronized","this","throw","throws","transient","try",
    "void","volatile","while", NULL
};

static const char *kw_js[] = {
    "await","break","case","catch","class","const","continue","debugger","default",
    "delete","do","else","enum","export","extends","false","finally","for","function",
    "if","import","in","instanceof","let","new","null","return","super","switch",
    "this","throw","true","try","typeof","var","void","while","with","yield", NULL
};

static const char *kw_ts[] = {
    "abstract","any","as","asserts","await","bigint","boolean","break","case","catch",
    "class","const","continue","declare","default","delete","do","else","enum","export",
    "extends","false","finally","for","from","function","get","if","implements","import",
    "in","infer","instanceof","interface","is","keyof","let","module","namespace","never",
    "new","null","number","object","package","private","protected","public","readonly",
    "return","set","static","string","super","switch","symbol","this","throw","true",
    "try","type","typeof","undefined","unique","unknown","var","void","while","with","yield", NULL
};

static const char *kw_py[] = {
    "and","as","assert","async","await","break","class","continue","def","del","elif",
    "else","except","False","finally","for","from","global","if","import","in","is",
    "lambda","None","nonlocal","not","or","pass","raise","return","True","try","while","with","yield", NULL
};

static const char *kw_pyspark[] = {
    "SparkSession","SparkContext","DataFrame","RDD","udf","col","lit","when","select",
    "filter","where","groupBy","agg","join","withColumn","read","write", NULL
};

static const char *kw_r[] = {
    "if","else","repeat","while","function","for","in","next","break","TRUE","FALSE","NULL","NA","NaN","Inf", NULL
};

static const char *kw_csharp[] = {
    "abstract","as","base","bool","break","byte","case","catch","char","checked",
    "class","const","continue","decimal","default","delegate","do","double","else","enum",
    "event","explicit","extern","false","finally","fixed","float","for","foreach","goto",
    "if","implicit","in","int","interface","internal","is","lock","long","namespace",
    "new","null","object","operator","out","override","params","private","protected",
    "public","readonly","ref","return","sbyte","sealed","short","sizeof","stackalloc",
    "static","string","struct","switch","this","throw","true","try","typeof","uint",
    "ulong","unchecked","unsafe","ushort","using","virtual","void","volatile","while", NULL
};

static const char *kw_julia[] = {
    "abstract","baremodule","begin","break","catch","const","continue","do","else","elseif",
    "end","export","false","finally","for","function","global","if","import","let","local",
    "macro","module","mutable","primitive","quote","return","struct","true","try","using","while", NULL
};

static const char *kw_perl[] = {
    "my","our","local","sub","use","package","if","elsif","else","unless","while","for",
    "foreach","continue","last","next","redo","return","undef","defined","eval","require", NULL
};

static const char *kw_matlab[] = {
    "break","case","catch","classdef","continue","else","elseif","end","for","function",
    "global","if","otherwise","parfor","persistent","return","switch","try","while", NULL
};

static const char *kw_kotlin[] = {
    "as","break","class","continue","do","else","false","for","fun","if","in","interface",
    "is","null","object","package","return","super","this","throw","true","try","typealias",
    "val","var","when","while", NULL
};

static const char *kw_php[] = {
    "abstract","and","array","as","break","callable","case","catch","class","clone","const",
    "continue","declare","default","do","echo","else","elseif","enddeclare","endfor","endforeach",
    "endif","endswitch","endwhile","extends","final","finally","for","foreach","function","global",
    "goto","if","implements","include","include_once","instanceof","interface","isset","list",
    "namespace","new","or","private","protected","public","require","require_once","return","static",
    "switch","throw","trait","try","unset","use","var","while","xor","yield", NULL
};

static const char *kw_ruby[] = {
    "BEGIN","END","alias","and","begin","break","case","class","def","defined?","do",
    "else","elsif","end","ensure","false","for","if","in","module","next","nil","not",
    "or","redo","rescue","retry","return","self","super","then","true","undef","unless",
    "until","when","while","yield", NULL
};

static const char *kw_rust[] = {
    "as","async","await","break","const","continue","crate","dyn","else","enum","extern",
    "false","fn","for","if","impl","in","let","loop","match","mod","move","mut","pub",
    "ref","return","self","Self","static","struct","super","trait","true","type","unsafe",
    "use","where","while","yield", NULL
};

static const char *kw_lua[] = {
    "and","break","do","else","elseif","end","false","for","function","goto","if","in",
    "local","nil","not","or","repeat","return","then","true","until","while", NULL
};

static const char *kw_sas[] = {
    "data","proc","run","quit","set","if","then","else","do","end","where","keep","drop",
    "merge","by","input","output","format","informat","length","label", NULL
};

static const char *kw_fortran[] = {
    "program","end","integer","real","double","precision","logical","character","dimension",
    "if","then","else","endif","do","enddo","stop","subroutine","function","return","module",
    "use","contains","implicit","none", NULL
};

static const char *kw_lisp[] = {
    "defun","defmacro","lambda","let","let*","if","cond","progn","quote","car","cdr","cons",
    "setq","setf","loop","when","unless","and","or","not", NULL
};

static const char *kw_scala[] = {
    "abstract","case","catch","class","def","do","else","extends","false","final","finally",
    "for","forSome","if","implicit","import","lazy","match","new","null","object","override",
    "package","private","protected","return","sealed","super","this","throw","trait","true",
    "try","type","val","var","while","with","yield", NULL
};

static const char *kw_asm[] = {
    "mov","add","sub","mul","div","jmp","je","jne","jg","jge","jl","jle","call","ret",
    "push","pop","cmp","and","or","xor","shl","shr","nop", NULL
};

static const char *kw_actionscript[] = {
    "break","case","catch","class","const","continue","default","delete","do","else","extends",
    "false","finally","for","function","if","implements","import","in","instanceof","interface",
    "new","null","override","private","protected","public","return","static","super","switch",
    "this","throw","true","try","typeof","var","while","with", NULL
};

static const char *kw_clojure[] = {
    "def","defn","defmacro","let","if","do","fn","loop","recur","when","cond","case",
    "->","->>","doseq","for","map","reduce","filter","nil","true","false", NULL
};

static const char *kw_coffeescript[] = {
    "and","or","is","isnt","not","class","extends","if","else","then","for","while",
    "until","loop","break","continue","return","try","catch","finally","throw","true","false",
    "null","undefined","new","super","this", NULL
};

static const char *kw_dart[] = {
    "abstract","as","assert","async","await","break","case","catch","class","const","continue",
    "covariant","default","deferred","do","dynamic","else","enum","export","extends","extension",
    "external","factory","false","final","finally","for","Function","get","hide","if","implements",
    "import","in","interface","late","library","mixin","new","null","on","operator","part",
    "required","rethrow","return","set","show","static","super","switch","this","throw","true",
    "try","typedef","var","void","while","with","yield", NULL
};

static const char *kw_cobol[] = {
    "IDENTIFICATION","DIVISION","PROGRAM-ID","ENVIRONMENT","DATA","PROCEDURE","SECTION","END-IF",
    "IF","ELSE","PERFORM","MOVE","ADD","SUBTRACT","MULTIPLY","DIVIDE","STOP","RUN", NULL
};

static const char *kw_elixir[] = {
    "def","defp","defmodule","do","end","if","else","case","cond","with","fn","receive",
    "try","catch","rescue","after","alias","import","require","use","true","false","nil", NULL
};

static const char *kw_groovy[] = {
    "as","assert","break","case","catch","class","const","continue","def","default","do","else",
    "enum","extends","false","finally","for","goto","if","implements","import","in","instanceof",
    "interface","new","null","package","return","super","switch","this","throw","trait","true","try",
    "while", NULL
};

static const char *kw_erlang[] = {
    "after","and","andalso","band","begin","bnot","bor","bsl","bsr","bxor","case","catch",
    "cond","div","end","fun","if","let","not","of","or","orelse","receive","rem","try","when","xor", NULL
};

static const char *kw_haskell[] = {
    "case","class","data","default","deriving","do","else","if","import","in","infix","infixl",
    "infixr","instance","let","module","newtype","of","then","type","where","forall", NULL
};

static const char *kw_pascal[] = {
    "and","array","begin","case","const","div","do","downto","else","end","file","for",
    "function","goto","if","in","label","mod","nil","not","of","or","packed","procedure",
    "program","record","repeat","set","then","to","type","until","var","while","with", NULL
};

static const char *kw_swift[] = {
    "associatedtype","class","deinit","enum","extension","fileprivate","func","import","init",
    "inout","internal","let","open","operator","private","protocol","public","static","struct",
    "subscript","typealias","var","break","case","continue","default","defer","do","else","fallthrough",
    "for","guard","if","in","repeat","return","switch","where","while","as","is","try","catch",
    "throw","nil","true","false", NULL
};

static const char *kw_scheme[] = {
    "define","lambda","let","let*","letrec","if","cond","case","begin","and","or","not",
    "quote","quasiquote","unquote","set!", NULL
};

static const char *kw_racket[] = {
    "#lang","define","lambda","let","let*","letrec","if","cond","case","begin","and","or",
    "not","require","provide","struct","module","match", NULL
};

static const char *kw_ocaml[] = {
    "and","as","assert","begin","class","constraint","do","done","downto","else","end","exception",
    "external","false","for","fun","function","functor","if","in","include","inherit","initializer",
    "lazy","let","match","method","module","mutable","new","object","of","open","or","private",
    "rec","sig","struct","then","to","true","try","type","val","virtual","when","while","with", NULL
};

static const char *kw_elm[] = {
    "if","then","else","case","of","let","in","type","module","import","exposing","as",
    "port","where", NULL
};

static const char *kw_haxe[] = {
    "abstract","break","case","cast","catch","class","const","continue","default","do","dynamic",
    "else","enum","extends","extern","false","final","for","function","if","implements","import",
    "in","inline","interface","macro","new","null","override","package","private","public",
    "return","static","super","switch","this","throw","true","try","typedef","var","while", NULL
};

static const char *kw_crystal[] = {
    "abstract","alias","as","asm","begin","break","case","class","def","do","else","elsif",
    "end","ensure","extend","false","for","fun","if","in","include","instance_sizeof","is_a?",
    "lib","macro","module","new","next","nil","not","or","out","private","protected","require",
    "rescue","responds_to?","return","self","sizeof","struct","super","then","true","type","typeof",
    "union","unless","until","when","while","with","yield", NULL
};

static const char *kw_fsharp[] = {
    "abstract","and","as","assert","base","begin","class","default","delegate","do","done","downcast",
    "downto","elif","else","end","exception","extern","false","finally","for","fun","function",
    "global","if","in","inherit","inline","interface","internal","lazy","let","match","member",
    "module","mutable","namespace","new","null","of","open","or","override","private","public",
    "rec","return","sig","static","struct","then","to","true","try","type","upcast","use","val",
    "void","when","while","with","yield", NULL
};

static const char *kw_tcl[] = {
    "after","append","array","break","catch","continue","dict","else","elseif","expr","for",
    "foreach","if","incr","join","lappend","lindex","list","proc","return","set","switch",
    "then","unset","while", NULL
};

static const char *kw_vbnet[] = {
    "AddHandler","AddressOf","And","AndAlso","As","Boolean","ByRef","Byte","ByVal","Call","Case",
    "Catch","Class","Const","Continue","Date","Decimal","Declare","Default","Delegate","Dim","Do",
    "Double","Each","Else","ElseIf","End","Enum","Erase","Error","Event","Exit","False","Finally",
    "For","Friend","Function","Get","GetType","GoSub","GoTo","Handles","If","Implements","Imports",
    "In","Inherits","Integer","Interface","Is","Let","Lib","Like","Long","Loop","Me","Mod",
    "Module","MustInherit","MustOverride","MyBase","MyClass","Namespace","New","Next","Not","Nothing",
    "NotInheritable","NotOverridable","Object","Of","On","Operator","Option","Optional","Or","OrElse",
    "Overloads","Overridable","Overrides","ParamArray","Private","Property","Protected","Public",
    "RaiseEvent","ReadOnly","ReDim","REM","RemoveHandler","Resume","Return","Select","Set","Shadows",
    "Shared","Short","Single","Static","Step","Stop","String","Structure","Sub","SyncLock","Then",
    "Throw","To","True","Try","TypeOf","UInteger","ULong","UShort","Using","When","While","With",
    "WithEvents","WriteOnly", NULL
};

static const char *kw_objc[] = {
    "@interface","@implementation","@end","@class","@protocol","@selector","@property","@synthesize",
    "@dynamic","@autoreleasepool","@try","@catch","@finally","@throw","@encode","@import",
    "@public","@protected","@private","@optional","@required","nil","YES","NO", NULL
};

static const char *kw_ada[] = {
    "abort","abs","abstract","accept","access","aliased","all","and","array","at","begin",
    "body","case","constant","declare","delay","delta","digits","do","else","elsif","end","entry",
    "exception","exit","for","function","generic","goto","if","in","interface","is","limited",
    "loop","mod","new","not","null","of","or","others","out","overriding","package","pragma",
    "private","procedure","protected","raise","range","record","rem","renames","requeue","return",
    "reverse","select","separate","subtype","tagged","task","terminate","then","type","until",
    "use","when","while","with","xor", NULL
};

static const char *kw_vala[] = {
    "abstract","as","base","bool","break","case","catch","char","class","const","construct",
    "continue","default","delegate","delete","do","double","else","enum","errordomain","extern",
    "false","finally","float","for","foreach","if","inline","int","interface","is","lock",
    "namespace","new","null","out","override","private","protected","public","ref","return",
    "short","signal","sizeof","static","string","struct","super","switch","this","throw","true",
    "try","typeof","uint","ulong","unowned","ushort","using","virtual","void","volatile","weak",
    "while","yield", NULL
};

static const char *kw_sql[] = {
    "SELECT","FROM","WHERE","JOIN","LEFT","RIGHT","INNER","OUTER","FULL","ON","GROUP","BY",
    "HAVING","ORDER","INSERT","INTO","VALUES","UPDATE","SET","DELETE","CREATE","ALTER","DROP",
    "TABLE","VIEW","INDEX","PRIMARY","KEY","FOREIGN","NOT","NULL","AS","DISTINCT","LIMIT",
    "OFFSET","UNION","ALL","CASE","WHEN","THEN","ELSE","END", NULL
};

static const char *kw_vb6[] = {
    "Dim","As","Integer","String","Long","Boolean","Sub","Function","End","If","Then","Else",
    "For","Next","While","Wend","Do","Loop","Select","Case","Return","Exit","Public","Private",
    "Set","New", NULL
};

static const char *kw_vba[] = {
    "Dim","As","Integer","String","Long","Boolean","Sub","Function","End","If","Then","Else",
    "For","Next","While","Wend","Do","Loop","Select","Case","Return","Exit","Public","Private",
    "Set","New","Option","Explicit","ByRef","ByVal", NULL
};

static const char *kw_vbscript[] = {
    "Dim","Set","If","Then","Else","For","Each","Next","While","Wend","Do","Loop","Select",
    "Case","Function","Sub","End","Class","Option","Explicit","On","Error","Resume","WScript", NULL
};

static const char *kw_powershell[] = {
    "function","param","begin","process","end","if","elseif","else","switch","foreach","for",
    "while","do","until","break","continue","return","throw","try","catch","finally",
    "class","enum","using","import","module","where","filter", NULL
};

static const char *kw_bash[] = {
    "if","then","else","elif","fi","for","while","do","done","case","esac","function",
    "select","in","time","coproc","return","break","continue","local","export","readonly", NULL
};

static const char *kw_delphi[] = {
    "and","array","begin","case","class","const","constructor","destructor","div","do","downto",
    "else","end","except","exports","file","finalization","finally","for","function","goto","if",
    "implementation","in","inherited","initialization","inline","interface","label","library","mod",
    "nil","not","object","of","or","packed","procedure","program","record","repeat","set","shl",
    "shr","then","to","try","type","unit","until","uses","var","while","with","xor", NULL
};

static const char *kw_zig[] = {
    "addrspace","align","allowzero","and","anyframe","anytype","asm","async","await","break",
    "catch","comptime","const","continue","defer","else","enum","errdefer","error","export","extern",
    "false","fn","for","if","inline","linksection","noalias","noinline","nosuspend","null","or",
    "orelse","packed","pub","resume","return","struct","suspend","switch","test","threadlocal",
    "true","try","union","unreachable","usingnamespace","var","volatile","while", NULL
};

static const char *kw_carbon[] = {
    "package","import","fn","var","let","if","else","while","for","return","struct","class",
    "interface","impl","match","as","type","choice","constraint","where","true","false", NULL
};

static const char *kw_nim[] = {
    "addr","and","as","asm","bind","block","break","case","cast","concept","const","continue",
    "converter","defer","discard","distinct","div","do","elif","else","end","enum","except",
    "export","finally","for","from","func","if","import","in","include","interface","is","isnot",
    "iterator","let","macro","method","mixin","mod","nil","not","notin","object","of","or",
    "out","proc","ptr","raise","ref","return","shl","shr","static","template","try","tuple",
    "type","using","var","when","while","with","without","xor","yield", NULL
};

static const char *kw_grain[] = {
    "let","var","fun","if","else","match","module","import","export","type","struct","enum",
    "pub","mut","true","false","switch","when","while","for","return", NULL
};

static const char *kw_gleam[] = {
    "const","fn","import","let","pub","type","case","assert","todo","panic","if","else",
    "true","false", NULL
};

static const char *kw_wren[] = {
    "break","class","construct","continue","else","false","for","foreign","if","import","in",
    "is","null","return","static","super","this","true","var","while", NULL
};

static const char *kw_janet[] = {
    "def","defn","defmacro","fn","let","if","do","while","for","break","continue","return",
    "nil","true","false","and","or", NULL
};

static const char *kw_oberon[] = {
    "MODULE","IMPORT","CONST","TYPE","VAR","PROCEDURE","BEGIN","END","IF","THEN","ELSE",
    "ELSIF","WHILE","DO","REPEAT","UNTIL","FOR","TO","BY","RETURN", NULL
};

static const char *kw_raku[] = {
    "class","role","grammar","module","sub","method","multi","my","our","state","has",
    "if","else","elsif","for","given","when","while","loop","return","next","last","redo",
    "use","require","constant","enum","subset","token","rule","regex","say","print","true","false", NULL
};

/* ---- Extensions ---- */
static const char *ext_c[] = {"c","h", NULL};
static const char *ext_cpp[] = {"cpp","cc","cxx","hpp","hxx","hh", NULL};
static const char *ext_d[] = {"d", NULL};
static const char *ext_go[] = {"go", NULL};
static const char *ext_java[] = {"java", NULL};
static const char *ext_js[] = {"js","mjs","cjs", NULL};
static const char *ext_ts[] = {"ts","tsx", NULL};
static const char *ext_py[] = {"py", NULL};
static const char *ext_r[] = {"r","R", NULL};
static const char *ext_csharp[] = {"cs", NULL};
static const char *ext_julia[] = {"jl", NULL};
static const char *ext_perl[] = {"pl","pm", NULL};
static const char *ext_matlab[] = {"m", NULL};
static const char *ext_kotlin[] = {"kt","kts", NULL};
static const char *ext_php[] = {"php", NULL};
static const char *ext_ruby[] = {"rb", NULL};
static const char *ext_rust[] = {"rs", NULL};
static const char *ext_lua[] = {"lua", NULL};
static const char *ext_sas[] = {"sas", NULL};
static const char *ext_fortran[] = {"f","for","f90","f95", NULL};
static const char *ext_lisp[] = {"lisp","lsp", NULL};
static const char *ext_scala[] = {"scala", NULL};
static const char *ext_asm[] = {"asm","s", NULL};
static const char *ext_actionscript[] = {"as", NULL};
static const char *ext_clojure[] = {"clj","cljs","cljc", NULL};
static const char *ext_coffeescript[] = {"coffee", NULL};
static const char *ext_dart[] = {"dart", NULL};
static const char *ext_cobol[] = {"cob","cbl", NULL};
static const char *ext_elixir[] = {"ex","exs", NULL};
static const char *ext_groovy[] = {"groovy","gvy","gy","gsh", NULL};
static const char *ext_erlang[] = {"erl","hrl", NULL};
static const char *ext_haskell[] = {"hs", NULL};
static const char *ext_pascal[] = {"pas","pp", NULL};
static const char *ext_swift[] = {"swift", NULL};
static const char *ext_scheme[] = {"scm","ss", NULL};
static const char *ext_racket[] = {"rkt", NULL};
static const char *ext_ocaml[] = {"ml","mli", NULL};
static const char *ext_elm[] = {"elm", NULL};
static const char *ext_haxe[] = {"hx", NULL};
static const char *ext_crystal[] = {"cr", NULL};
static const char *ext_fsharp[] = {"fs","fsi","fsx", NULL};
static const char *ext_tcl[] = {"tcl", NULL};
static const char *ext_vbnet[] = {"vb", NULL};
static const char *ext_objc[] = {"mm", NULL};
static const char *ext_ada[] = {"adb","ads", NULL};
static const char *ext_vala[] = {"vala","vapi", NULL};
static const char *ext_sql[] = {"sql", NULL};
static const char *ext_vb6[] = {"frm","bas","cls", NULL};
static const char *ext_vba[] = {"vba", NULL};
static const char *ext_vbscript[] = {"vbs", NULL};
static const char *ext_powershell[] = {"ps1","psm1","psd1", NULL};
static const char *ext_bash[] = {"sh","bash", NULL};
static const char *ext_delphi[] = {"dpr","dpk", NULL};
static const char *ext_zig[] = {"zig", NULL};
static const char *ext_carbon[] = {"carbon", NULL};
static const char *ext_nim[] = {"nim", NULL};
static const char *ext_grain[] = {"gr", NULL};
static const char *ext_gleam[] = {"gleam", NULL};
static const char *ext_wren[] = {"wren", NULL};
static const char *ext_janet[] = {"janet","jdn", NULL};
static const char *ext_oberon[] = {"obn","obp","mod", NULL};
static const char *ext_raku[] = {"raku","rakumod","pm6","p6", NULL};

/* ---- Language registry ---- */
#define SH_STR_SQ_DQ "\"'"
#define SH_STR_DQ "\""
#define SH_STR_SQ_DQ_BT "\"'`"

#define SH_LANG(name, exts, kw, lc, bcs, bce, strs, flags) \
    { (name), (exts), (kw), (lc), (bcs), (bce), (strs), (flags) }

static const SyntaxLang sh_langs[] = {
    SH_LANG("C", ext_c, kw_c, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("C++", ext_cpp, kw_cpp, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("D", ext_d, kw_d, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("Golang", ext_go, kw_go, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("Java", ext_java, kw_java, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("JavaScript", ext_js, kw_js, "//", "/*", "*/", SH_STR_SQ_DQ_BT, 0),
    SH_LANG("TypeScript", ext_ts, kw_ts, "//", "/*", "*/", SH_STR_SQ_DQ_BT, 0),
    SH_LANG("Python", ext_py, kw_py, "#", NULL, NULL, SH_STR_SQ_DQ, 0),
    SH_LANG("PySpark", ext_py, kw_pyspark, "#", NULL, NULL, SH_STR_SQ_DQ, 0),
    SH_LANG("R", ext_r, kw_r, "#", NULL, NULL, SH_STR_SQ_DQ, 0),
    SH_LANG("Csharp", ext_csharp, kw_csharp, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("Julia", ext_julia, kw_julia, "#", "#=", "=#", SH_STR_SQ_DQ, 0),
    SH_LANG("Perl", ext_perl, kw_perl, "#", NULL, NULL, SH_STR_SQ_DQ, 0),
    SH_LANG("Matlab", ext_matlab, kw_matlab, "%", "%{", "%}", SH_STR_SQ_DQ, 0),
    SH_LANG("Kotlin", ext_kotlin, kw_kotlin, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("PHP", ext_php, kw_php, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("Ruby", ext_ruby, kw_ruby, "#", NULL, NULL, SH_STR_SQ_DQ, 0),
    SH_LANG("Rust", ext_rust, kw_rust, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("Lua", ext_lua, kw_lua, "--", "--[[", "]]", SH_STR_SQ_DQ, 0),
    SH_LANG("SAS", ext_sas, kw_sas, NULL, "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("Fortran", ext_fortran, kw_fortran, "!", NULL, NULL, SH_STR_SQ_DQ, SH_FLAG_KW_CASE_INSENSITIVE),
    SH_LANG("Lisp", ext_lisp, kw_lisp, ";", "#|", "|#", SH_STR_DQ, 0),
    SH_LANG("Scala", ext_scala, kw_scala, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("Assembly", ext_asm, kw_asm, ";", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("ActionScript", ext_actionscript, kw_actionscript, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("Clojure", ext_clojure, kw_clojure, ";", NULL, NULL, SH_STR_DQ, 0),
    SH_LANG("CoffeeScript", ext_coffeescript, kw_coffeescript, "#", "###", "###", SH_STR_SQ_DQ_BT, 0),
    SH_LANG("Dart", ext_dart, kw_dart, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("COBOL", ext_cobol, kw_cobol, "*>", NULL, NULL, SH_STR_SQ_DQ, SH_FLAG_KW_CASE_INSENSITIVE),
    SH_LANG("Elixir", ext_elixir, kw_elixir, "#", NULL, NULL, SH_STR_SQ_DQ, 0),
    SH_LANG("Groovy", ext_groovy, kw_groovy, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("Erlang", ext_erlang, kw_erlang, "%", NULL, NULL, SH_STR_SQ_DQ, 0),
    SH_LANG("Haskell", ext_haskell, kw_haskell, "--", "{-", "-}", SH_STR_SQ_DQ, 0),
    SH_LANG("Pascal", ext_pascal, kw_pascal, "//", "{", "}", SH_STR_SQ_DQ, SH_FLAG_KW_CASE_INSENSITIVE),
    SH_LANG("Swift", ext_swift, kw_swift, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("Scheme", ext_scheme, kw_scheme, ";", "#|", "|#", SH_STR_DQ, 0),
    SH_LANG("Racket", ext_racket, kw_racket, ";", "#|", "|#", SH_STR_DQ, 0),
    SH_LANG("OCaml", ext_ocaml, kw_ocaml, NULL, "(*", "*)", SH_STR_DQ, 0),
    SH_LANG("Elm", ext_elm, kw_elm, "--", "{-", "-}", SH_STR_SQ_DQ, 0),
    SH_LANG("Haxe", ext_haxe, kw_haxe, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("Crystal", ext_crystal, kw_crystal, "#", NULL, NULL, SH_STR_SQ_DQ, 0),
    SH_LANG("Fsharp", ext_fsharp, kw_fsharp, "//", "(*", "*)", SH_STR_SQ_DQ, 0),
    SH_LANG("Tcl", ext_tcl, kw_tcl, "#", NULL, NULL, SH_STR_SQ_DQ, 0),
    SH_LANG("VB.NET", ext_vbnet, kw_vbnet, "\'", NULL, NULL, SH_STR_DQ, SH_FLAG_KW_CASE_INSENSITIVE),
    SH_LANG("Objective_C", ext_objc, kw_objc, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("Ada", ext_ada, kw_ada, "--", NULL, NULL, SH_STR_SQ_DQ, SH_FLAG_KW_CASE_INSENSITIVE),
    SH_LANG("Vala", ext_vala, kw_vala, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("SQL", ext_sql, kw_sql, "--", "/*", "*/", SH_STR_SQ_DQ, SH_FLAG_KW_CASE_INSENSITIVE),
    SH_LANG("VB6", ext_vb6, kw_vb6, "\'", NULL, NULL, SH_STR_DQ, SH_FLAG_KW_CASE_INSENSITIVE),
    SH_LANG("VBA", ext_vba, kw_vba, "\'", NULL, NULL, SH_STR_DQ, SH_FLAG_KW_CASE_INSENSITIVE),
    SH_LANG("VBScript", ext_vbscript, kw_vbscript, "\'", NULL, NULL, SH_STR_DQ, SH_FLAG_KW_CASE_INSENSITIVE),
    SH_LANG("PowerShell", ext_powershell, kw_powershell, "#", "<#", "#>", SH_STR_SQ_DQ, SH_FLAG_KW_CASE_INSENSITIVE),
    SH_LANG("Bash", ext_bash, kw_bash, "#", NULL, NULL, SH_STR_SQ_DQ, 0),
    SH_LANG("Delphi", ext_delphi, kw_delphi, "//", "{", "}", SH_STR_SQ_DQ, SH_FLAG_KW_CASE_INSENSITIVE),
    SH_LANG("Zig", ext_zig, kw_zig, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("Carbon", ext_carbon, kw_carbon, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("Nim", ext_nim, kw_nim, "#", "#[", "]#", SH_STR_SQ_DQ, 0),
    SH_LANG("Grain", ext_grain, kw_grain, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("Gleam", ext_gleam, kw_gleam, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("Wren", ext_wren, kw_wren, "//", "/*", "*/", SH_STR_SQ_DQ, 0),
    SH_LANG("Janet", ext_janet, kw_janet, "#", NULL, NULL, SH_STR_SQ_DQ, 0),
    SH_LANG("Oberon+", ext_oberon, kw_oberon, NULL, "(*", "*)", SH_STR_DQ, SH_FLAG_KW_CASE_INSENSITIVE),
    SH_LANG("Raku", ext_raku, kw_raku, "#", NULL, NULL, SH_STR_SQ_DQ, 0),
};

static inline const SyntaxLang *sh_lang_for_file(const char *path) {
    const char *ext = sh_ext_from_path(path);
    if (!ext) return NULL;
    if (strcmp(ext, "py") == 0) {
        if (sh_contains_ci(path, "spark") || sh_contains_ci(path, "pyspark")) {
            for (unsigned int i = 0; i < sizeof(sh_langs)/sizeof(sh_langs[0]); i++) {
                if (strcmp(sh_langs[i].name, "PySpark") == 0) return &sh_langs[i];
            }
        }
    }
    for (unsigned int i = 0; i < sizeof(sh_langs)/sizeof(sh_langs[0]); i++) {
        if (sh_ext_matches(ext, sh_langs[i].exts)) return &sh_langs[i];
    }
    return NULL;
}

#endif
