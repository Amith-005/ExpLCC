%{
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include "compiler.h"
    #include "symbol_table.h"

    int yylex(void);
    void yyerror(const char* s);
    extern FILE *yyin;

    FILE* out = NULL;          // target file (a.xsm)
    int decl_type = -1;        // type of the declaration currently being parsed
    char func_label[20];       // label of the function currently being parsed
    int cur_type = -1;         // user-defined type currently being defined
    int cur_class = -1;        // class currently being defined (type code)
    int in_class = 0;          // 1 while the body of a class is being parsed
    int methods_started = 0;   // 1 once a method declaration has been seen in the current class
    FILE* methods_out = NULL;  // code of the methods (they appear before the global declarations)

    /* The startup code must come first, so the method code is written to a side file and appended after it */
    FILE* codeOut() { return in_class ? methods_out : out; }

    void flushMethods() {
        char buf[1024];
        size_t n;
        rewind(methods_out);
        while((n = fread(buf, 1, sizeof(buf), methods_out)) > 0) fwrite(buf, 1, n, out);
        fclose(methods_out);
        remove("methods.tmp");
        methods_out = NULL;
    }

    void declError(const char* fmt, const char* name) {
        char msg[200];
        snprintf(msg, sizeof(msg), fmt, name);
        yyerror(msg);
        exit(1);
    }

    void installGlobal(char* name, int type, int size, int* metadata) {
        if(Install(name, type, size, metadata)) {
            declError("redeclaration of %s", name);
        }
    }

    void installLocal(char* name, int type) {
        if(LInstall(name, type)) {
            declError("redeclaration of local variable %s", name);
        }
    }

    /* Name equivalence: definition must match declaration in number, names and types of parameters */
    void checkParams(char* fname, Paramstruct* decl, Paramstruct* def) {
        while(decl != NULL && def != NULL) {
            if(strcmp(decl->name, def->name) != 0 || decl->type != def->type) {
                declError("parameters of %s do not match its declaration", fname);
            }
            decl = decl->next;
            def = def->next;
        }
        if(decl != NULL || def != NULL) {
            declError("number of parameters of %s does not match its declaration", fname);
        }
    }

    void installField(char* name, int type) {
        int r = TAddField(cur_type, name, type);
        if(r == 1) declError("duplicate member field %s", name);
        if(r == 2) declError("too many member fields in type %s", typeName(cur_type));
    }

    int resolveType(char* name) {
        int t = TLookup(name);
        if(t < 0) t = CLookup(name);
        if(t < 0) declError("undefined type %s", name);
        return t;
    }

    void checkNotArrayOfUser(int type, char* name) {
        if(isUserType(type) || isClassType(type)) declError("arrays of user-defined type are not supported (%s)", name);
    }

    tnode* appendArg(tnode* list, tnode* expr) {
        tnode* arg = createNode(0, expr->type, ARG_NODE, NULL, expr, NULL, NULL);
        if(list == NULL) return arg;
        tnode* temp = list;
        while(temp->right != NULL) temp = temp->right;
        temp->right = arg;
        return list;
    }

    /* Checks a method definition header against its declaration; self is at BP-3, arguments start at BP-4 */
    void beginMethod(char* name, int type, Paramstruct* params) {
        Method* m = Class_Mlookup(cur_class, name);
        if(m == NULL) declError("method %s is not declared in its class", name);
        if(m->defined) declError("redefinition of method %s", name);
        if(m->type != type) declError("return type of %s does not match its declaration", name);
        checkParams(name, m->paramlist, params);
        m->defined = 1;

        LReset();
        LInstallSelf(cur_class);
        if(LInstallParams(params, -4)) declError("duplicate parameter name in %s", name);
        setCurrentFunction(NULL, type);
        snprintf(func_label, sizeof(func_label), "F%d", m->flabel);
    }

    void checkAllMethodsDefined() {
        Method* m;
        for(m = Class_Entry(cur_class)->methods; m != NULL; m = m->next) {
            if(!m->defined) declError("method %s declared but not defined", m->name);
        }
    }

    /* Checks a function definition header against its declaration and sets up its local symbol table */
    void beginFunction(char* name, int type, Paramstruct* params) {
        Gsymbol* f = Lookup(name);
        if(f == NULL || !f->isFunction) declError("function %s is not declared", name);
        if(f->defined) declError("redefinition of function %s", name);
        if(f->type != type) declError("return type of %s does not match its declaration", name);
        checkParams(name, f->paramlist, params);
        f->defined = 1;

        LReset();
        if(LInstallParams(params, -3)) declError("duplicate parameter name in %s", name);
        setCurrentFunction(f, f->type);
        snprintf(func_label, sizeof(func_label), "F%d", f->flabel);
    }

    void checkAllFunctionsDefined() {
        Gsymbol* g = get_global_head();
        while(g != NULL) {
            if(g->isFunction && !g->defined) {
                declError("function %s declared but not defined", g->name);
            }
            g = g->next;
        }
    }
%}

%union {
    tnode *node;
    int ival;
    char* str;
    Paramstruct* plist;
}

%type <node> FieldRef MethodCall DeleteStmt stmt_list stmt expr AsgStmt OutputStmt InputStmt Ifstmt Whilestmt DoWhilestmt RepeatUntilstmt ReturnStmt VarNode LValue Body ArgList CallExpr
%type <ival> Type
%type <plist> ParamList NEParamList Param
%token TYPE ENDTYPE NULLTOK ALLOC FREE INITIALIZE NBEGIN END READ WRITE EQ GT NE LT LE GE IF WHILE ENDIF DO ENDWHILE THEN ELSE REPEAT UNTIL DECL ENDDECL INT STR RETURN MAIN AND OR NOT BREAK CONTINUE CLASS ENDCLASS NEW DELETE SELF
%token <str> ID STRING_TOKEN
%token <ival> NUM

%left OR
%left AND
%right NOT
%nonassoc LT GT LE GE EQ NE
%left '+' '-'
%left '*' '/' '%'
%right U_STAR

%%

program : TypeDefOpt ClassDefOpt GdeclOpt FdefBlock MainBlock   { checkAllFunctionsDefined(); }
        | TypeDefOpt ClassDefOpt GdeclOpt MainBlock             { checkAllFunctionsDefined(); }
        ;

/* ---------------- Type definitions ---------------- */

TypeDefOpt : TYPE TypeDefList ENDTYPE   { print_type_table(); }
           | /* empty */
           ;

TypeDefList : TypeDefList TypeDef
            | TypeDef
            ;

TypeDef : TypeName '{' FieldDeclList '}'
        ;

TypeName : ID {
            cur_type = TCreate($1);
            if(cur_type < 0) declError("redefinition of type %s", $1);
        }
         ;

FieldDeclList : FieldDeclList FieldDecl
              | FieldDecl
              ;

FieldDecl : Type ID ';'   { installField($2, $1); }
          ;

/* ---------------- Class definitions ---------------- */

ClassDefOpt : CLASS ClassDefList ENDCLASS   { print_class_table(); }
            | /* empty */
            ;

ClassDefList : ClassDefList ClassDef
             | ClassDef
             ;

ClassDef : ClassName '{' DECL MemberList ENDDECL MethodDefs '}' {
                checkAllMethodsDefined();
                in_class = 0;
                setCurrentClass(0);
            }
         ;

ClassName : ID {
                cur_class = CInstall($1);
                if(cur_class < 0) declError("redefinition of class %s", $1);
                in_class = 1;
                methods_started = 0;
            }
          ;

MemberList : MemberList Member
           | /* empty */
           ;

Member : Type ID ';' {
                if(methods_started) declError("member field %s must be declared before the methods", $2);
                int r = Class_Finstall(cur_class, $2, $1);
                if(r == 1) declError("duplicate member field %s", $2);
                if(r == 2) declError("too many member fields in class %s", typeName(cur_class));
            }
       | Type ID '(' ParamList ')' ';' {
                methods_started = 1;
                if(isClassType($1)) declError("method %s cannot return a class", $2);
                if(Class_Minstall(cur_class, $2, $1, $4) == NULL) {
                    declError("duplicate method name or too many methods (%s)", $2);
                }
            }
       ;

MethodDefs : MethodDefs MethodDef
           | /* empty */
           ;

MethodDef : MethodHeader '{' LdeclBlock Body '}' {
                print_local_table(func_label);
                genFunction(func_label, LLocalCount(), $4, codeOut());
            }
          ;

MethodHeader : Type ID '(' ParamList ')'  { setCurrentClass(cur_class); beginMethod($2, $1, $4); }
             ;

/* ---------------- Global declarations ---------------- */

GdeclOpt : GdeclBlock   { print_symbol_table(); genStartup(out); flushMethods(); }
         | /* empty */          { genStartup(out); flushMethods(); }
         ;

GdeclBlock : DECL GdeclList ENDDECL
           | DECL ENDDECL
           ;

GdeclList : GdeclList GDecl
          | GDecl
          ;

GDecl : Type { decl_type = $1; } GidList ';'
      ;

GidList : GidList ',' Gid
        | Gid
        ;

Gid : ID                        { installGlobal($1, decl_type, 1, NULL); }
    | '*' ID                    { installGlobal($2, pointerTo(decl_type), 1, NULL); }
    | ID '[' NUM ']' {
            int* metadata = (int*)malloc(sizeof(int));
            metadata[0] = $3;
            checkNotArrayOfUser(decl_type, $1);
            int arr_type = (decl_type == INTEGER_TYPE) ? INTEGER_ARRAY_TYPE : STRING_ARRAY_TYPE;
            installGlobal($1, arr_type, $3, metadata);
        }
    | ID '[' NUM ']' '[' NUM ']' {
            int* metadata = (int*)malloc(2 * sizeof(int));
            metadata[0] = $3;
            metadata[1] = $6;
            checkNotArrayOfUser(decl_type, $1);
            int arr_type = (decl_type == INTEGER_TYPE) ? INTEGER_ARRAY_TYPE : STRING_ARRAY_TYPE;
            installGlobal($1, arr_type, $3 * $6, metadata);
        }
    | ID '(' ParamList ')' {
            if(isClassType(decl_type)) declError("function %s cannot return a class", $1);
            if(InstallFunction($1, decl_type, $3)) {
                declError("redeclaration of %s", $1);
            }
        }
    ;

ParamList : NEParamList   { $$ = $1; }
          | /* empty */   { $$ = NULL; }
          ;

NEParamList : NEParamList ',' Param  { $$ = appendParam($1, $3); }
            | Param                  { $$ = $1; }
            ;

Param : Type ID       {
            if(isClassType($1)) declError("class variable %s cannot be an argument", $2);
            $$ = makeParam($2, $1);
        }
      ;

Type : INT { $$ = INTEGER_TYPE; }
     | STR { $$ = STRING_TYPE; }
     | ID  { $$ = resolveType($1); }
     ;

/* ---------------- Function definitions ---------------- */

FdefBlock : FdefBlock Fdef
          | Fdef
          ;

Fdef : FHeader '{' LdeclBlock Body '}' {
            print_local_table(func_label);
            genFunction(func_label, LLocalCount(), $4, out);
        }
     ;

FHeader : Type ID '(' ParamList ')'      { beginFunction($2, $1, $4); }
        ;

MainBlock : MainHeader '{' LdeclBlock Body '}' {
            print_local_table("MAIN");
            genFunction("MAIN", LLocalCount(), $4, out);
        }
          ;

MainHeader : INT MAIN '(' ')' {
            LReset();
            setCurrentFunction(NULL, INTEGER_TYPE);
            strcpy(func_label, "MAIN");
        }
           ;

/* ---------------- Local declarations ---------------- */

LdeclBlock : DECL LDecList ENDDECL
           | DECL ENDDECL
           | /* empty */
           ;

LDecList : LDecList LDecl
         | LDecl
         ;

LDecl : Type { decl_type = $1; } IdList ';'
      ;

IdList : IdList ',' LId
       | LId
       ;

LId : ID        { installLocal($1, decl_type); }
    ;

Body : NBEGIN stmt_list END  { $$ = $2; }
     | NBEGIN END            { $$ = NULL; }
     ;

/* ---------------- Statements ---------------- */

stmt_list : stmt_list stmt ';' { $$ = createNode(0, INTEGER_TYPE, STATEMENT, NULL, $1, $2, NULL); }
          | stmt ';' { $$ = $1; }
          ;

stmt : InputStmt | OutputStmt | AsgStmt | Ifstmt | Whilestmt | DoWhilestmt | RepeatUntilstmt | ReturnStmt | DeleteStmt
     | BREAK { $$ = createNode(0, 0, BREAK_NODE, NULL, NULL, NULL, NULL); }
     | CONTINUE { $$ = createNode(0, 0, CONTINUE_NODE, NULL, NULL, NULL, NULL); }
     ;

AsgStmt : LValue '=' expr {
            $$ = createNode(0, INTEGER_TYPE, ASSIGNMENT, NULL, $1, $3, NULL);
        }
        ;

LValue : VarNode { $$ = $1; }
       | '*' expr %prec U_STAR { $$ = createNode(0, INTEGER_TYPE, DEREF_NODE, NULL, $2, NULL, NULL); }
       ;

OutputStmt : WRITE '(' expr ')' {
                $$ = createNode(0, INTEGER_TYPE, WRITE_NODE, NULL, $3, NULL, NULL);
             }
             ;

InputStmt : READ '(' VarNode ')' {
                $$ = createNode(0, INTEGER_TYPE, READ_NODE, NULL, $3, NULL, NULL);
            }
            ;

DeleteStmt : DELETE '(' VarNode ')' {
                $$ = createNode(0, INTEGER_TYPE, DELETE_NODE, NULL, $3, NULL, NULL);
             }
             ;

ReturnStmt : RETURN expr {
                $$ = createNode(0, INTEGER_TYPE, RETURN_NODE, NULL, $2, NULL, NULL);
             }
             ;

Ifstmt : IF '(' expr ')' THEN stmt_list ELSE stmt_list ENDIF { $$ = createNode(0, BOOLEAN_TYPE, IF_NODE, NULL, $3, $8, $6); }
       | IF '(' expr ')' THEN stmt_list ENDIF { $$ = createNode(0, BOOLEAN_TYPE, IF_NODE, NULL, $3, NULL, $6); }
       ;

Whilestmt : WHILE '(' expr ')' DO stmt_list ENDWHILE { $$ = createNode(0, BOOLEAN_TYPE, WHILE_NODE, NULL, $3, $6, NULL); } ;

DoWhilestmt : DO stmt_list WHILE '(' expr ')' { $$ = createNode(0, BOOLEAN_TYPE, DO_WHILE_NODE, NULL, $5, $2, NULL); } ;

RepeatUntilstmt : REPEAT stmt_list UNTIL '(' expr ')' { $$ = createNode(0, BOOLEAN_TYPE, REPEAT_UNTIL_NODE, NULL, $5, $2, NULL); } ;

/* ---------------- Expressions ---------------- */

expr : expr '+' expr  { $$ = createNode(0, INTEGER_TYPE, PLUS_NODE, NULL, $1, $3, NULL); }
     | expr '-' expr  { $$ = createNode(0, INTEGER_TYPE, MINUS_NODE, NULL, $1, $3, NULL); }
     | expr '*' expr  { $$ = createNode(0, INTEGER_TYPE, MUL_NODE, NULL, $1, $3, NULL); }
     | expr '%' expr  { $$ = createNode(0, INTEGER_TYPE, MODULUS_NODE, NULL, $1, $3, NULL); }
     | expr '/' expr  { $$ = createNode(0, INTEGER_TYPE, DIV_NODE, NULL, $1, $3, NULL); }
     | expr GT expr   { $$ = createNode(0, BOOLEAN_TYPE, GT_NODE, NULL, $1, $3, NULL); }
     | expr EQ expr   { $$ = createNode(0, BOOLEAN_TYPE, EQ_NODE, NULL, $1, $3, NULL); }
     | expr GE expr   { $$ = createNode(0, BOOLEAN_TYPE, GE_NODE, NULL, $1, $3, NULL); }
     | expr LE expr   { $$ = createNode(0, BOOLEAN_TYPE, LE_NODE, NULL, $1, $3, NULL); }
     | expr NE expr   { $$ = createNode(0, BOOLEAN_TYPE, NE_NODE, NULL, $1, $3, NULL); }
     | expr LT expr   { $$ = createNode(0, BOOLEAN_TYPE, LT_NODE, NULL, $1, $3, NULL); }
     | expr AND expr  { $$ = createNode(0, BOOLEAN_TYPE, AND_NODE, NULL, $1, $3, NULL); }
     | expr OR expr   { $$ = createNode(0, BOOLEAN_TYPE, OR_NODE, NULL, $1, $3, NULL); }
     | NOT expr       { $$ = createNode(0, BOOLEAN_TYPE, NOT_NODE, NULL, $2, NULL, NULL); }
     | '*' expr %prec U_STAR { $$ = createNode(0, INTEGER_TYPE, DEREF_NODE, NULL, $2, NULL, NULL); }
     | '&' VarNode    { $$ = createNode(0, INTEGER_POINTER_TYPE, ADDRESS_NODE, NULL, $2, NULL, NULL); }
     | '(' expr ')'   { $$ = $2; }
     | NUM            { $$ = createNode($1, INTEGER_TYPE, CONSTANT, NULL, NULL, NULL, NULL); }
     | '-' NUM        { $$ = createNode(-$2, INTEGER_TYPE, CONSTANT, NULL, NULL, NULL, NULL); }
     | VarNode        { $$ = $1; }
     | NULLTOK        { $$ = createNode(0, NULL_TYPE, CONSTANT, NULL, NULL, NULL, NULL); }
     | ALLOC '(' ')'  { $$ = createNode(0, ALLOC_TYPE, ALLOC_NODE, NULL, NULL, NULL, NULL); }
     | INITIALIZE '(' ')' { $$ = createNode(0, INTEGER_TYPE, INIT_NODE, NULL, NULL, NULL, NULL); }
     | FREE '(' expr ')'  { $$ = createNode(0, INTEGER_TYPE, FREE_NODE, NULL, $3, NULL, NULL); }
     | STRING_TOKEN   { $$ = createNode(0, STRING_TYPE, STRING_CONSTANT_NODE, $1, NULL, NULL, NULL); }
     | CallExpr       { $$ = $1; }
     | NEW '(' ID ')' { $$ = makeNewNode($3); }
     ;

CallExpr : ID '(' ')'         { $$ = makeCallNode($1, NULL); }
         | ID '(' ArgList ')' { $$ = makeCallNode($1, $3); }
         | MethodCall         { $$ = $1; }
         ;

MethodCall : SELF '.' ID '(' ')'             { $$ = makeMethodCallNode(makeSelfNode(), $3, NULL); }
           | SELF '.' ID '(' ArgList ')'     { $$ = makeMethodCallNode(makeSelfNode(), $3, $5); }
           | ID '.' ID '(' ')'               { $$ = makeMethodCallNode(makeVarNode($1, NULL, NULL), $3, NULL); }
           | ID '.' ID '(' ArgList ')'       { $$ = makeMethodCallNode(makeVarNode($1, NULL, NULL), $3, $5); }
           | FieldRef '.' ID '(' ')'         { $$ = makeMethodCallNode($1, $3, NULL); }
           | FieldRef '.' ID '(' ArgList ')' { $$ = makeMethodCallNode($1, $3, $5); }
           ;

ArgList : ArgList ',' expr { $$ = appendArg($1, $3); }
        | expr             { $$ = appendArg(NULL, $1); }
        ;

VarNode : ID                            { $$ = makeVarNode($1, NULL, NULL); }
        | ID '[' expr ']'               { $$ = makeVarNode($1, $3, NULL); }
        | ID '[' expr ']' '[' expr ']'  { $$ = makeVarNode($1, $3, $6); }
        | FieldRef                      { $$ = $1; }
        ;

FieldRef : SELF '.' ID      { $$ = makeFieldNode(makeSelfNode(), $3); }
         | ID '.' ID        { $$ = makeFieldNode(makeVarNode($1, NULL, NULL), $3); }
         | FieldRef '.' ID  { $$ = makeFieldNode($1, $3); }
         ;

%%

void yyerror(const char *s) {
    extern char *yytext;
    extern int yylineno;
    if(yytext)
        printf("Error at token '%s' (line %d): %s\n", yytext, yylineno, s);
    else
        printf("Error: %s\n", s);
}

int main(int argc, char* argv[]) {
    char* fname = (argc > 1) ? argv[1] : "input.txt";
    FILE* fp = fopen(fname, "r");
    if(!fp) {
        printf("No file found... exiting\n");
        exit(1);
    }
    yyin = fp;

    methods_out = fopen("methods.tmp", "w+");
    if(!methods_out) {
        printf("File couldn't be opened exiting\n");
        exit(1);
    }

    out = fopen("a.xsm", "w");
    if(!out) {
        printf("File couldn't be opened exiting\n");
        exit(1);
    }

    if(yyparse() != 0) {
        fclose(out);
        exit(1);
    }

    fclose(out);
    printf("Code generated in a.xsm\n");
    return 0;
}
