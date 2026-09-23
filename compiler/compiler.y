%{
#include <stdlib.h>
#include <stdio.h>
#include "compiler.h"

int yylex(void);
void yyerror(const char *s);
FILE *fptr;
%}

%union {
    tnode *node;
    int integer;
}

%token <integer> NUM
%type <node> expr
%left '+' '-'
%left '*' '/'

%%

start:
      expr '\n' {
          initializeRegs();
          fptr = fopen("output.xsm", "w");
          if (!fptr) {
              perror("fopen");
              exit(1);
          }

          fprintf(fptr, "0\n2056\n0\n0\n0\n0\n0\n0\n");
          int resultReg = codeGen($1, fptr);
          fprintf(fptr, "MOV [4096], R%d\n", resultReg);
          fprintf(fptr, "MOV R0, [4096]\n");
          fprintf(fptr, "MOV SP, 4095\n");
          fprintf(fptr, "MOV R1, \"Write\"\n");
          fprintf(fptr, "PUSH R1\n");
          fprintf(fptr, "MOV R1, -2\n");
          fprintf(fptr, "PUSH R1\n");
          fprintf(fptr, "PUSH R0\n");
          fprintf(fptr, "PUSH R1\n");
          fprintf(fptr, "PUSH R1\n");
          fprintf(fptr, "CALL 0\n");
          fprintf(fptr, "POP R0\n");
          fprintf(fptr, "POP R1\n");
          fprintf(fptr, "POP R1\n");
          fprintf(fptr, "POP R1\n");
          fprintf(fptr, "POP R1\n");
          fprintf(fptr, "MOV R1, \"Exit\"\n");
          fprintf(fptr, "PUSH R1\n");
          fprintf(fptr, "MOV R1, -2\n");
          fprintf(fptr, "PUSH R1\n");
          fprintf(fptr, "PUSH R1\n");
          fprintf(fptr, "PUSH R1\n");
          fprintf(fptr, "PUSH R1\n");
          fprintf(fptr, "BRKP\n");
          fprintf(fptr, "CALL 0\n");

          fclose(fptr);
          exit(0);
      }
    ;

expr:
      expr '+' expr { $$ = makeOperatorNode('+', $1, $3); }
    | expr '-' expr { $$ = makeOperatorNode('-', $1, $3); }
    | expr '*' expr { $$ = makeOperatorNode('*', $1, $3); }
    | expr '/' expr { $$ = makeOperatorNode('/', $1, $3); }
    | '(' expr ')'  { $$ = $2; }
    | NUM           { $$ = makeLeafNode($1); }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "yyerror: %s\n", s);
}

int main(void) {
    return yyparse();
}
