%{
    #include <stdio.h>
    #include "compiler.h"
    #include "y.tab.h"
    #include <stdlib.h>
    #include <string.h>

    void yyerror(const char* s);
%}

%option yylineno

%%

"begin"     {return NBEGIN;}
"end"       {return END;}
"read"      {return READ;}
"write"     {return WRITE;}
"if"        {return IF;}
"repeat"    {return REPEAT;}
"until"     {return UNTIL;}
"while"     {return WHILE;}
"endif"     {return ENDIF;}
"do"        {return DO;}
"endwhile"  {return ENDWHILE;}
"continue"  {return CONTINUE;}
"break"     {return BREAK;}
"then"      {return THEN;}
"else"      {return ELSE;}
"int"       {return INT;}
"str"       {return STR;}
"decl"      {return DECL;}
"enddecl"   {return ENDDECL;}
"return"    {return RETURN;}
"main"      {return MAIN;}
"tuple"     {return TUPLE;}
"AND"       {return AND;}
"OR"        {return OR;}
"NOT"       {return NOT;}

"\""[^"\n"]*"\"" { yylval.str = strdup(yytext); return STRING_TOKEN; }

[a-zA-Z_][a-zA-Z0-9_]* {
    yylval.str = strdup(yytext);
    return ID;
}

[0-9]+ {
    yylval.ival = atoi(yytext);
    return NUM;
}

">=" {return GE;}
"<=" {return LE;}
"==" {return EQ;}
"!=" {return NE;}
"->" {return ARROW;}
"&&" {return AND;}
"||" {return OR;}
">"  {return GT;}
"<"  {return LT;}
"!"  {return NOT;}

"//".* {}
[ \t\r\n]+ {}
[\x80-\xFF]+ {}
. { return yytext[0]; }

%%

int yywrap() {
    return 1;
}
