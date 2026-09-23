%{
#include <stdlib.h>
#include <stdio.h>
#include "compiler.h"
#include "y.tab.h"
int number;
%}

DIGIT   [0-9]
NUM     {DIGIT}+

%%

{NUM}      { number = atoi(yytext); yylval.integer = number; return NUM; }
"+"        { return '+'; }
"-"        { return '-'; }
"*"        { return '*'; }
"/"        { return '/'; }
"("        { return '('; }
")"        { return ')'; }
[ \t]+     { }
[\n]       { return '\n'; }
.          { fprintf(stderr, "Unknown char: %s\n", yytext); exit(1); }

%%

int yywrap(void) { return 1; }
