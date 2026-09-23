%{
#include <stdlib.h>
#include <stdio.h>
#include "compiler.h"
#include "compiler.c"

extern FILE* yyin;
extern char* yytext;
extern int yylineno;

void yyerror(char const *s);
int yylex(void);
%}

%union{
    struct AST_Node *node;
}


%token PLUS_ MINUS_ DIV_ LT_ GT_ LE_ GE_ NE_ EQ_ STAR_ AMPERSAND_
%token BEGIN_ END_ READ_ WRITE_ IF_ THEN_ ELSE_ ENDIF_ WHILE_ DO_ ENDWHILE_ REPEAT_ UNTIL_
%token INT_ STR_ DECL_ ENDDECL_
%token ID_ NUM_ TEXT_ ID_DECL_
%token BREAK_ CONTINUE_       

%type <node> program Declarations decl_list decl var_list var_item l_value 
%type <node> stmt_list stmt InputStmt OutputStmt AsgStmt IfStmt WhileStmt RepeatUntilStmt DoWhileStmt
%type <node> expr id
%type <node> ID_ NUM_ TEXT_ BREAK_ CONTINUE_ 

%left LT_ GT_ LE_ GE_ NE_ EQ_
%left PLUS_ MINUS_
%left STAR_ DIV_
%right UMINUS USTAR UAMP

%%

program : Declarations BEGIN_ stmt_list END_ {
                $$ = $3;
                GSTPrint();
                //print_tree($$, 0, 0);
                FILE *fp = fopen("output.xsm", "w");
				fprintf(fp, "0\n2056\n0\n0\n0\n0\n0\n0\n");
				fprintf(fp, "MOV SP, 4122\n");
				int p = codeGen($3, fp);
				fprintf(fp, "INT 10\n");
                exit(0);
            }
        | Declarations BEGIN_ END_ {
            printf("Empty Program\n");
            printf("Parsing Successful\n");
            exit(1); 
            };

Declarations: DECL_ decl_list ENDDECL_ {}
            | DECL_ ENDDECL_ {$$ = NULL;};

decl_list: decl_list decl
          | decl;

decl: INT_ var_list ';' {ASTChangeType($2, INTEGER);}
    | STR_ var_list ';' {ASTChangeType($2, STRING);};

var_list: var_list ',' var_item { $$ = makeNode(STATEMENT, VOID, $1, NULL, $3, "VARLIST"); }
        | var_item { $$ = $1; };

var_item: ID_ {
            GSTInstall($1->varname, $1->type, 1, 0, 0, $1->type); // Simple variable
            $$ = $1;
        }
        | STAR_ ID_ { // Pointer declaration: *p
            GSTInstall($2->varname, VOID, 1, 0, 0, $2->type);
            $$ = $2;
        }
        | ID_ '[' NUM_ ']' { // 1D Array
            GSTInstall($1->varname, $1->type, $3->val, 0, 1, $1->type);
            $$ = $1;
        }
        | ID_ '[' NUM_ ']' '[' NUM_ ']' { // 2D Array
            GSTInstall($1->varname, $1->type, $3->val, $6->val, 2, $1->type);
            $$ = $1;
        };

stmt_list: stmt_list stmt ';' {$$ = makeNode(STATEMENT, VOID, $1, NULL, $2, "STATEMENT");}
          | stmt ';' {$$ = $1;};

stmt: InputStmt
    | OutputStmt
    | AsgStmt
    | IfStmt
    | WhileStmt
    | RepeatUntilStmt
    | DoWhileStmt
    | BREAK_
    | CONTINUE_;

InputStmt: READ_ '(' l_value ')' { $$ = makeNode(READ, VOID, $3, NULL, NULL, "READ");}; 

OutputStmt: WRITE_ '(' expr ')' { $$ = makeNode(WRITE, VOID, $3, NULL, NULL, "WRITE");};

AsgStmt: l_value '=' expr { $$ = makeNode(OPERATOR, INTEGER, $1, NULL, $3, "="); };

IfStmt: IF_ '(' expr ')' THEN_ stmt_list ELSE_ stmt_list ENDIF_ { $$ = makeNode(IF, VOID, $3, $6, $8, "IF");}
      | IF_ '(' expr ')' THEN_ stmt_list ENDIF_ { $$ = makeNode(IF, VOID, $3, $6, NULL, "IF");};

WhileStmt: WHILE_ '(' expr ')' DO_ stmt_list ENDWHILE_ { $$ = makeNode(WHILE, VOID, $3, NULL, $6, "WHILE");};

RepeatUntilStmt: REPEAT_ stmt_list UNTIL_ '(' expr ')' { $$ = makeNode(REPEAT, VOID, $2, NULL, $5, "REPEAT"); };

DoWhileStmt: DO_ stmt_list WHILE_ '(' expr ')' { $$ = makeNode(DOWHILE, VOID, $2, NULL, $5, "DOWHILE"); };

expr : expr PLUS_ expr      {$$ = makeNode(OPERATOR, INTEGER, $1, NULL, $3, "+");}
      | expr MINUS_ expr    {$$ = makeNode(OPERATOR, INTEGER, $1, NULL, $3, "-");}
      | expr STAR_ expr     {$$ = makeNode(OPERATOR, INTEGER, $1, NULL, $3, "*");}
      | expr DIV_ expr      {$$ = makeNode(OPERATOR, INTEGER, $1, NULL, $3, "/");}
      | expr LT_ expr       {$$ = makeNode(OPERATOR, BOOLEAN, $1, NULL, $3, "<");}
      | expr GT_ expr       {$$ = makeNode(OPERATOR, BOOLEAN, $1, NULL, $3, ">");}
      | expr LE_ expr       {$$ = makeNode(OPERATOR, BOOLEAN, $1, NULL, $3, "<=");}
      | expr GE_ expr       {$$ = makeNode(OPERATOR, BOOLEAN, $1, NULL, $3, ">=");}
      | expr NE_ expr       {$$ = makeNode(OPERATOR, BOOLEAN, $1, NULL, $3, "!=");}
      | expr EQ_ expr       {$$ = makeNode(OPERATOR, BOOLEAN, $1, NULL, $3, "==");}
      | '(' expr ')'        {$$ = $2;}
      | NUM_                {$$ = $1;}
      | id                  {$$ = $1;} 
      | TEXT_               {$$ = $1;}
      | MINUS_ expr %prec UMINUS {
          $$ = makeNode(OPERATOR, INTEGER, makeConstantLeafNode(INTEGER, 0, "0"), NULL, $2, "-");
      }
      | STAR_ expr %prec USTAR { 
            $$ = makePointerNode(POINTER, $2, "*");
        }
      | AMPERSAND_ id %prec UAMP { 
            $$ = makePointerNode(ADDRESS, $2, "&");
        };

l_value: id { $$ = $1; } 
       | STAR_ expr %prec USTAR { 
             $$ = makePointerNode(POINTER, $2, "*");
         };

id: ID_ {
        $$ = $1;
        struct GST_Node *curr = GSTLookup($1->varname);
        if (curr == NULL) {
            printf("Variable \"%s\" not declared\n", $1->varname);
            exit(1);
        }
        $$->type = curr->type;
    }
    | ID_ '[' expr ']' { 
        if ($3->type == BOOLEAN) {
            printf("Array index cannot be boolean\n");
            exit(1);
        }
        struct GST_Node *curr = GSTLookup($1->varname);
        if (curr == NULL) {
            printf("Array \"%s\" not declared\n", $1->varname);
            exit(1);
        }
        if (!curr->dimensions) {
            printf("\"%s\" is not an array\n", $1->varname);
            exit(1);
        }
        $1->type = curr->type;
        $$ = makeArrayLeafNode($1->varname, $3, "ARRAY");
    }
    | ID_ '[' expr ']' '[' expr ']' { 
        struct GST_Node *curr = GSTLookup($1->varname);
        if (curr == NULL) {
            printf("Array \"%s\" not declared\n", $1->varname);
            exit(1);
        }
        if (curr->dimensions != 2) {
            printf("\"%s\" is not a 2D array\n", $1->varname);
            exit(1);
        }
        if ($3->type != INTEGER || $6->type != INTEGER) {
            printf("Array indices must be integers\n");
            exit(1);
        }
        $1->type = curr->type;
        $$ = makeArray2DLeafNode($1->varname, $3, $6, "ARRAY2D");
    };

%%

void yyerror(char const *s) {
    printf("yyerror | Line: %d\n%s: %s\n", yylineno, s, yytext);
    exit(1);
}

int main(void) {
   char fname[50];
   printf("Input file name:\n");

   if (scanf("%49s", fname) != 1) {
       printf("Error reading filename.\n");
       return 1;
   }
   yyin=fopen(fname,"r");
   if (!yyin) {
       printf("Error: Could not open file %s\n", fname);
       return 1;
   }
   yyparse();
   return 0;
}
