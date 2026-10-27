#include "compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

extern void yyerror(const char *s);
int genLibCall(char* code, int arg1_reg, int arg1_const, FILE* target_file);

int reg_index = -1;
int label_index = -1;

int break_labels[20];
int continue_labels[20];
int loop_top = -1;

/* Function currently being parsed (for return type checking) */
Gsymbol* current_function = NULL;
int current_return_type = INTEGER_TYPE;

/* Class whose methods are being parsed (0 outside a class) */
int current_class = 0;

void semanticError(const char* fmt, const char* name) {
    char msg[200];
    snprintf(msg, sizeof(msg), fmt, name);
    yyerror(msg);
    exit(1);
}

int getLabel() {
    label_index++;
    return label_index;
}

int getReg() {
    if(reg_index < 19) {
        reg_index++;
        return reg_index;
    }
    printf("No registers available\n");
    exit(1);
}

void freeReg() {
    if(reg_index >= 0) {
        reg_index--;
    }
}

void setCurrentFunction(Gsymbol* f, int returnType) {
    current_function = f;
    current_return_type = returnType;
}

void setCurrentClass(int code) {
    current_class = code;
}

int getCurrentClass() {
    return current_class;
}

int getCurrentReturnType() {
    return current_return_type;
}

int pointerTo(int base_type) {
    if(base_type == INTEGER_TYPE) return INTEGER_POINTER_TYPE;
    if(base_type == STRING_TYPE) return STRING_POINTER_TYPE;
    yyerror("invalid pointer base type");
    exit(1);
}

/* Name equivalence: types must be identical; NULL is compatible with every user-defined type */
static int argCompatible(int lt, int rt) {
    return lt == rt || ((isUserType(lt) || isClassType(lt)) && rt == NULL_TYPE);
}

static int assignable(int lt, int rt) {
    return argCompatible(lt, rt) || (isUserType(lt) && rt == ALLOC_TYPE);
}

static int comparable(int lt, int rt) {
    if(lt == ALLOC_TYPE || rt == ALLOC_TYPE) return 0;
    return argCompatible(lt, rt) || argCompatible(rt, lt);
}

static int isArrayType(int type) {
    return type == INTEGER_ARRAY_TYPE || type == STRING_ARRAY_TYPE;
}

static int isPointerType(int type) {
    return type == INTEGER_POINTER_TYPE || type == STRING_POINTER_TYPE;
}

tnode* createNode(int val, int type, int nodetype, char* varname, tnode *l, tnode *r, tnode* m) {
    tnode* newNode = (tnode*)malloc(sizeof(tnode));
    newNode->val = val;
    newNode->left = l;
    newNode->right = r;
    newNode->mid = m;
    newNode->type = type;
    newNode->varname = varname != NULL ? strdup(varname) : NULL;
    newNode->nodetype = nodetype;
    newNode->Gentry = NULL;
    newNode->Lentry = NULL;
    newNode->Mentry = NULL;

    switch(nodetype) {
        case PLUS_NODE: case MINUS_NODE: case MUL_NODE: case DIV_NODE: case MODULUS_NODE:
            if(l->type != INTEGER_TYPE || r->type != INTEGER_TYPE) {
                yyerror("type mismatch: arithmetic operands must be integers");
                exit(1);
            }
            newNode->type = INTEGER_TYPE;
            break;

        case LT_NODE: case GT_NODE: case LE_NODE: case GE_NODE: case EQ_NODE: case NE_NODE:
            if(!comparable(l->type, r->type) || isArrayType(l->type) || l->type == BOOLEAN_TYPE) {
                yyerror("type mismatch in comparison");
                exit(1);
            }
            newNode->type = BOOLEAN_TYPE;
            break;

        case AND_NODE: case OR_NODE:
            if(l->type != BOOLEAN_TYPE || r->type != BOOLEAN_TYPE) {
                yyerror("type mismatch: logical operands must be boolean");
                exit(1);
            }
            newNode->type = BOOLEAN_TYPE;
            break;

        case NOT_NODE:
            if(l->type != BOOLEAN_TYPE) {
                yyerror("type mismatch: operand of not must be boolean");
                exit(1);
            }
            newNode->type = BOOLEAN_TYPE;
            break;

        case IF_NODE: case WHILE_NODE: case DO_WHILE_NODE: case REPEAT_UNTIL_NODE:
            if(l->type != BOOLEAN_TYPE) {
                yyerror("type mismatch: condition must be boolean");
                exit(1);
            }
            break;

        case DEREF_NODE:
            if(l->type == INTEGER_POINTER_TYPE) newNode->type = INTEGER_TYPE;
            else if(l->type == STRING_POINTER_TYPE) newNode->type = STRING_TYPE;
            else {
                yyerror("type mismatch: dereferencing a non-pointer");
                exit(1);
            }
            break;

        case ADDRESS_NODE:
            if(l->nodetype != VARIABLE || (l->type != INTEGER_TYPE && l->type != STRING_TYPE)) {
                yyerror("& can only be applied to an int/str variable");
                exit(1);
            }
            newNode->type = pointerTo(l->type);
            break;

        case ASSIGNMENT:
            if(isArrayType(l->type)) {
                yyerror("cannot assign value directly to an array name");
                exit(1);
            }
            if(!assignable(l->type, r->type)) {
                yyerror("type mismatch in assignment");
                exit(1);
            }
            break;

        case WRITE_NODE:
            if(l->type != INTEGER_TYPE && l->type != STRING_TYPE && !isPointerType(l->type)) {
                yyerror("write expects an int or str expression");
                exit(1);
            }
            break;

        case READ_NODE:
            if(l->type != INTEGER_TYPE && l->type != STRING_TYPE) {
                yyerror("read expects an int or str variable");
                exit(1);
            }
            break;

        case FREE_NODE:
            if(!isUserType(l->type)) {
                yyerror("free expects a variable of a user-defined type");
                exit(1);
            }
            break;

        case DELETE_NODE:
            if(!isUserType(l->type) && !isClassType(l->type)) {
                yyerror("delete expects a variable of a user-defined type or a class");
                exit(1);
            }
            break;

        case RETURN_NODE:
            if(!argCompatible(current_return_type, l->type)) {
                yyerror("type mismatch: return type does not match function type");
                exit(1);
            }
            break;
    }

    return newNode;
}

tnode* makeFieldNode(tnode* base, char* field) {
    if(isClassType(base->type)) {
        /* member fields of a class are private: only self.field is permitted */
        if(base->nodetype != VARIABLE || base->Lentry == NULL || strcmp(base->varname, "self") != 0) {
            semanticError("member field %s of a class can be accessed only through self", field);
        }
        Field* cf = Class_Flookup(base->type, field);
        if(cf == NULL) semanticError("no member field named %s", field);
        return createNode(cf->index, cf->type, FIELD_NODE, field, base, NULL, NULL);
    }
    if(!isUserType(base->type)) semanticError("%s is not a variable of a user-defined type", base->varname);
    Field* f = FLookup(base->type, field);
    if(f == NULL) semanticError("no member field named %s", field);
    return createNode(f->index, f->type, FIELD_NODE, field, base, NULL, NULL);
}

/* Look up the local symbol table first, then the global symbol table */
tnode* makeVarNode(char* name, tnode* index1, tnode* index2) {
    tnode* node = createNode(0, INTEGER_TYPE, VARIABLE, name, index1, index2, NULL);

    Lsymbol* L = LLookup(name);
    if(L != NULL) {
        if(index1 != NULL) semanticError("%s is not an array", name);
        node->Lentry = L;
        node->type = L->type;
        return node;
    }

    if(current_class != 0) semanticError("undeclared variable %s (a method can use only its arguments, locals and self.fields)", name);

    Gsymbol* G = Lookup(name);
    if(G == NULL) semanticError("undeclared variable %s", name);
    if(G->isFunction) semanticError("%s is a function, not a variable", name);
    node->Gentry = G;

    if(isArrayType(G->type)) {
        if(index1 == NULL) {
            node->type = G->type;      // bare array name
        } else {
            if(index1->type != INTEGER_TYPE || (index2 && index2->type != INTEGER_TYPE)) {
                semanticError("array index of %s must be an integer", name);
            }
            node->type = (G->type == INTEGER_ARRAY_TYPE) ? INTEGER_TYPE : STRING_TYPE;
        }
    } else {
        if(index1 != NULL) semanticError("%s is not an array", name);
        node->type = G->type;
    }
    return node;
}

tnode* makeCallNode(char* name, tnode* args) {
    if(current_class != 0) semanticError("a method cannot call the global function %s", name);
    Gsymbol* G = Lookup(name);
    if(G == NULL || !G->isFunction) semanticError("call to undeclared function %s", name);

    /* check number and types of arguments against the declaration */
    Paramstruct* p = G->paramlist;
    tnode* a = args;
    while(p != NULL && a != NULL) {
        if(!argCompatible(p->type, a->left->type)) semanticError("type mismatch in argument to %s", name);
        p = p->next;
        a = a->right;
    }
    if(p != NULL || a != NULL) semanticError("wrong number of arguments in call to %s", name);

    tnode* node = createNode(0, G->type, FUNC_CALL_NODE, name, args, NULL, NULL);
    node->Gentry = G;
    return node;
}

tnode* makeSelfNode() {
    Lsymbol* L = (current_class != 0) ? LLookup("self") : NULL;
    if(L == NULL) semanticError("%s can be used only inside a method", "self");
    tnode* node = createNode(0, L->type, VARIABLE, "self", NULL, NULL, NULL);
    node->Lentry = L;
    return node;
}

/* Name equivalence on the signature; the implicit self is not part of the argument list */
tnode* makeMethodCallNode(tnode* object, char* method, tnode* args) {
    if(!isClassType(object->type)) semanticError("method %s invoked on a non-class expression", method);
    Method* m = Class_Mlookup(object->type, method);
    if(m == NULL) semanticError("no method named %s in the class", method);

    Paramstruct* p = m->paramlist;
    tnode* a = args;
    while(p != NULL && a != NULL) {
        if(!argCompatible(p->type, a->left->type)) semanticError("type mismatch in argument to %s", method);
        p = p->next;
        a = a->right;
    }
    if(p != NULL || a != NULL) semanticError("wrong number of arguments in call to %s", method);

    tnode* node = createNode(0, m->type, METHOD_CALL_NODE, method, args, NULL, object);
    node->Mentry = m;
    return node;
}

tnode* makeNewNode(char* classname) {
    int code = CLookup(classname);
    if(code < 0) semanticError("new: undefined class %s", classname);
    return createNode(code, code, NEW_NODE, classname, NULL, NULL, NULL);
}

/* Computes the memory address of a variable (or array element) into a register */
int getVarAddressReg(tnode *varNode, FILE *target_file) {
    // Member field: address = (value of the base reference) + field offset
    if(varNode->nodetype == FIELD_NODE) {
        int base_reg = codeGen(varNode->left, target_file);
        if(varNode->val != 0) fprintf(target_file, "ADD R%d, %d\n", base_reg, varNode->val);
        return base_reg;
    }

    int addr_reg = getReg();

    // Local variable / parameter: address = BP + binding
    if(varNode->Lentry != NULL) {
        int b = varNode->Lentry->binding;
        fprintf(target_file, "MOV R%d, BP\n", addr_reg);
        if(b >= 0) fprintf(target_file, "ADD R%d, %d\n", addr_reg, b);
        else fprintf(target_file, "SUB R%d, %d\n", addr_reg, -b);
        return addr_reg;
    }

    int base_addr = varNode->Gentry->binding;
    fprintf(target_file, "MOV R%d, %d\n", addr_reg, base_addr);

    // 1D Array: index = left
    if(varNode->left != NULL && varNode->right == NULL) {
        int off_reg = codeGen(varNode->left, target_file);
        fprintf(target_file, "ADD R%d, R%d\n", addr_reg, off_reg);
        freeReg();
    }

    // 2D Array
    else if(varNode->left != NULL && varNode->right != NULL) {
        int r_idx = codeGen(varNode->left, target_file);
        int c_idx = codeGen(varNode->right, target_file);
        int cols = varNode->Gentry->metadata[1];

        fprintf(target_file, "MUL R%d, %d\n", r_idx, cols);
        fprintf(target_file, "ADD R%d, R%d\n", r_idx, c_idx);
        fprintf(target_file, "ADD R%d, R%d\n", addr_reg, r_idx);

        freeReg();
        freeReg();
    }

    return addr_reg;
}

/* Address of anything that can be assigned to / read into / have its address taken */
int getLValueAddr(tnode* node, FILE* target_file) {
    if(node->nodetype == DEREF_NODE) return codeGen(node->left, target_file);
    return getVarAddressReg(node, target_file);
}

/* Push arguments in reverse order: the last argument is pushed first,
   so that argument 1 ends up at [BP-3], argument 2 at [BP-4], ... */
void pushArgs(tnode* arg, FILE* target_file) {
    if(arg == NULL) return;
    pushArgs(arg->right, target_file);
    int r = codeGen(arg->left, target_file);
    fprintf(target_file, "PUSH R%d\n", r);
    freeReg();
}

int genFunctionCall(tnode* t, FILE* target_file) {
    int saved = reg_index;
    int i;

    // 1. caller saves registers in use
    for(i = 0; i <= saved; i++) {
        fprintf(target_file, "PUSH R%d\n", i);
    }

    // 2. evaluate and push arguments
    pushArgs(t->left, target_file);

    // 3. push space for the return value
    int tmp = getReg();
    fprintf(target_file, "PUSH R%d\n", tmp);
    freeReg();

    // 4. call
    fprintf(target_file, "CALL F%d\n", t->Gentry->flabel);

    // 5. after return: fetch return value into a fresh register
    int ret_reg = getReg();
    fprintf(target_file, "POP R%d\n", ret_reg);

    // 6. pop the arguments
    int nargs = countParams(t->Gentry->paramlist);
    if(nargs > 0) {
        tmp = getReg();
        for(i = 0; i < nargs; i++) {
            fprintf(target_file, "POP R%d\n", tmp);
        }
        freeReg();
    }

    // 7. restore saved registers
    for(i = saved; i >= 0; i--) {
        fprintf(target_file, "POP R%d\n", i);
    }

    return ret_reg;
}

/* Method invocation. Same convention as a function call, except that the object (self) is
   pushed after the other arguments, so that it ends up at [BP-3] with argument 1 at [BP-4]. */
int genMethodCall(tnode* t, FILE* target_file) {
    int saved = reg_index;
    int i;

    for(i = 0; i <= saved; i++) {
        fprintf(target_file, "PUSH R%d\n", i);
    }

    pushArgs(t->left, target_file);

    int obj = codeGen(t->mid, target_file);
    fprintf(target_file, "PUSH R%d\n", obj);              // self
    freeReg();

    int tmp = getReg();
    fprintf(target_file, "PUSH R%d\n", tmp);              // space for the return value
    freeReg();

    fprintf(target_file, "CALL F%d\n", t->Mentry->flabel);

    int ret_reg = getReg();
    fprintf(target_file, "POP R%d\n", ret_reg);

    int nargs = countParams(t->Mentry->paramlist) + 1;    // + self
    tmp = getReg();
    for(i = 0; i < nargs; i++) {
        fprintf(target_file, "POP R%d\n", tmp);
    }
    freeReg();

    for(i = saved; i >= 0; i--) {
        fprintf(target_file, "POP R%d\n", i);
    }

    return ret_reg;
}

/* Calls a library function through the ABI: CALL 0 with the function code, three arguments and an
   empty slot for the return value on the stack (the same interface as Read/Write/Exit).
   Registers in use are saved around the call. Returns a fresh register holding the return value. */
int genLibCall(char* code, int arg1_reg, int arg1_const, FILE* target_file) {
    int saved = reg_index;
    int i;
    for(i = 0; i <= saved; i++) fprintf(target_file, "PUSH R%d\n", i);
    int t = getReg();
    int res = getReg();
    fprintf(target_file, "MOV R%d, \"%s\"\n", t, code);
    fprintf(target_file, "PUSH R%d\n", t);               // function code
    if(arg1_reg >= 0) {
        fprintf(target_file, "PUSH R%d\n", arg1_reg);    // argument 1
    } else {
        fprintf(target_file, "MOV R%d, %d\n", t, arg1_const);
        fprintf(target_file, "PUSH R%d\n", t);
    }
    fprintf(target_file, "PUSH R%d\n", t);               // argument 2 (unused)
    fprintf(target_file, "PUSH R%d\n", t);               // argument 3 (unused)
    fprintf(target_file, "PUSH R%d\n", t);               // space for the return value
    fprintf(target_file, "CALL 0\n");
    fprintf(target_file, "POP R%d\n", res);              // return value
    for(i = 0; i < 4; i++) fprintf(target_file, "POP R%d\n", t);
    for(i = saved; i >= 0; i--) fprintf(target_file, "POP R%d\n", i);
    fprintf(target_file, "MOV R%d, R%d\n", t, res);
    freeReg();                                           // res
    return t;
}

void genReturnSequence(FILE* target_file) {
    fprintf(target_file, "MOV SP, BP\n");
    fprintf(target_file, "POP BP\n");
    fprintf(target_file, "RET\n");
}

int codeGen(tnode *t, FILE *target_file) {
    if (t == NULL) return -1;
    if (t->nodetype == STATEMENT) {
        codeGen(t->left, target_file);
        codeGen(t->right, target_file);
        return -1;
    }

    if (t->nodetype == CONSTANT) {
        int r = getReg();
        fprintf(target_file, "MOV R%d, %d\n", r, t->val);
        return r;
    }

    if(t->nodetype == DEREF_NODE) {
        int reg = codeGen(t->left, target_file);
        fprintf(target_file, "MOV R%d, [R%d]\n", reg, reg);
        return reg;
    }

    if(t->nodetype == ADDRESS_NODE) {
        return getLValueAddr(t->left, target_file);
    }

    if (t->nodetype == VARIABLE) {
        int r = getReg();
        int addr_reg = getVarAddressReg(t, target_file);
        fprintf(target_file, "MOV R%d, [R%d]\n", r, addr_reg);
        freeReg(); // free addr_reg
        return r;
    }

    if(t->nodetype == FIELD_NODE) {
        int r = getVarAddressReg(t, target_file);
        fprintf(target_file, "MOV R%d, [R%d]\n", r, r);
        return r;
    }

    if(t->nodetype == ALLOC_NODE) {
        return genLibCall("Alloc", -1, 8, target_file);        // size = fixed block size
    }

    if(t->nodetype == INIT_NODE) {
        return genLibCall("Heapset", -1, 0, target_file);
    }

    if(t->nodetype == FREE_NODE) {
        int a = codeGen(t->left, target_file);
        int r = genLibCall("Free", a, 0, target_file);
        fprintf(target_file, "MOV R%d, R%d\n", a, r);
        freeReg();
        return a;
    }

    if(t->nodetype == NEW_NODE) {
        return genLibCall("Alloc", -1, 8, target_file);        // 8 words for the member fields
    }

    if(t->nodetype == DELETE_NODE) {
        int a = codeGen(t->left, target_file);
        genLibCall("Free", a, 0, target_file);
        freeReg();  // result of Free
        freeReg();  // a
        return -1;
    }

    if(t->nodetype == METHOD_CALL_NODE) {
        return genMethodCall(t, target_file);
    }

    if(t->nodetype == FUNC_CALL_NODE) {
        return genFunctionCall(t, target_file);
    }

    if(t->nodetype == RETURN_NODE) {
        int r = codeGen(t->left, target_file);
        int addr = getReg();
        // return value goes to [BP-2]
        fprintf(target_file, "MOV R%d, BP\n", addr);
        fprintf(target_file, "SUB R%d, 2\n", addr);
        fprintf(target_file, "MOV [R%d], R%d\n", addr, r);
        freeReg();
        freeReg();
        genReturnSequence(target_file);
        return -1;
    }

    if(t->nodetype == IF_NODE) {
        int regno = codeGen(t->left, target_file);
        int else_label = getLabel();
        int end_label = getLabel();

        fprintf(target_file, "JZ R%d, L%d\n", regno, else_label);
        freeReg();

        if(t->mid != NULL) codeGen(t->mid, target_file);
        fprintf(target_file, "JMP L%d\n", end_label);

        fprintf(target_file, "L%d:\n", else_label);
        if(t->right != NULL) codeGen(t->right, target_file);

        fprintf(target_file, "L%d:\n", end_label);
        return -1;
    }

    if(t->nodetype == WHILE_NODE) {
        int while_start = getLabel();
        int while_end = getLabel();

        fprintf(target_file, "L%d:\n", while_start);
        int reg_no = codeGen(t->left, target_file);
        fprintf(target_file, "JZ R%d, L%d\n", reg_no, while_end);
        freeReg();

        loop_top++;
        continue_labels[loop_top] = while_start;
        break_labels[loop_top] = while_end;

        codeGen(t->right, target_file);

        loop_top--;

        fprintf(target_file, "JMP L%d\n", while_start);
        fprintf(target_file, "L%d:\n", while_end);
        return -1;
    }

    if(t->nodetype == DO_WHILE_NODE) {
        int do_start = getLabel();
        int do_continue = getLabel();
        int do_end = getLabel();

        fprintf(target_file, "L%d:\n", do_start);

        loop_top++;
        continue_labels[loop_top] = do_continue;
        break_labels[loop_top] = do_end;

        codeGen(t->right, target_file);
        loop_top--;

        fprintf(target_file, "L%d:\n", do_continue);
        int reg_no = codeGen(t->left, target_file);
        fprintf(target_file, "JZ R%d, L%d\n", reg_no, do_end);
        freeReg();

        fprintf(target_file, "JMP L%d\n", do_start);
        fprintf(target_file, "L%d:\n", do_end);
        return -1;
    }

    if(t->nodetype == REPEAT_UNTIL_NODE) {
        int repeat_start = getLabel();
        int repeat_continue = getLabel();
        int repeat_end = getLabel();

        fprintf(target_file, "L%d:\n", repeat_start);

        loop_top++;
        continue_labels[loop_top] = repeat_continue;
        break_labels[loop_top] = repeat_end;

        codeGen(t->right, target_file);
        loop_top--;

        fprintf(target_file, "L%d:\n", repeat_continue);
        int reg_no = codeGen(t->left, target_file);
        fprintf(target_file, "JZ R%d, L%d\n", reg_no, repeat_start);
        freeReg();

        fprintf(target_file, "L%d:\n", repeat_end);
        return -1;
    }

    if (t->nodetype == ASSIGNMENT) {
        int right_reg = codeGen(t->right, target_file);
        int addr_reg = getLValueAddr(t->left, target_file);
        fprintf(target_file, "MOV [R%d], R%d\n", addr_reg, right_reg);
        freeReg(); // free addr_reg
        freeReg(); // free right_reg
        return -1;
    }

    if(t->nodetype == BREAK_NODE) {
        if(loop_top >= 0) {
            fprintf(target_file, "JMP L%d\n", break_labels[loop_top]);
        }
        return -1;
    }

    if(t->nodetype == CONTINUE_NODE) {
        if(loop_top >= 0) {
            fprintf(target_file, "JMP L%d\n", continue_labels[loop_top]);
        }
        return -1;
    }

    if(t->nodetype == WRITE_NODE) {
        int expr_reg = codeGen(t->left, target_file);
        int temp = getReg();

        fprintf(target_file, "MOV R%d, \"Write\"\n", temp);
        fprintf(target_file, "PUSH R%d\n", temp);
        fprintf(target_file, "MOV R%d, -2\n", temp);
        fprintf(target_file, "PUSH R%d\n", temp);
        fprintf(target_file, "MOV R%d, R%d\n", temp, expr_reg);
        fprintf(target_file, "PUSH R%d\n", temp);
        fprintf(target_file, "PUSH R%d\n", temp);
        fprintf(target_file, "PUSH R%d\n", temp);

        fprintf(target_file, "CALL 0\n");

        fprintf(target_file, "POP R%d\n", temp);
        fprintf(target_file, "POP R%d\n", temp);
        fprintf(target_file, "POP R%d\n", temp);
        fprintf(target_file, "POP R%d\n", temp);
        fprintf(target_file, "POP R%d\n", temp);

        freeReg();  // Free temp
        freeReg();  // Free expr_reg
        return -1;
    }

    if(t->nodetype == READ_NODE) {
        int temp = getReg();
        int addr_reg = getLValueAddr(t->left, target_file);

        fprintf(target_file, "MOV R%d, \"Read\"\n", temp);
        fprintf(target_file, "PUSH R%d\n", temp);
        fprintf(target_file, "MOV R%d, -1\n", temp);
        fprintf(target_file, "PUSH R%d\n", temp);
        fprintf(target_file, "MOV R%d, R%d\n", temp, addr_reg);
        fprintf(target_file, "PUSH R%d\n", temp);
        fprintf(target_file, "PUSH R%d\n", temp);
        fprintf(target_file, "PUSH R%d\n", temp);

        fprintf(target_file, "CALL 0\n");

        fprintf(target_file, "POP R%d\n", temp);
        fprintf(target_file, "POP R%d\n", temp);
        fprintf(target_file, "POP R%d\n", temp);
        fprintf(target_file, "POP R%d\n", temp);
        fprintf(target_file, "POP R%d\n", temp);

        freeReg(); // free addr_reg
        freeReg(); // free temp
        return -1;
    }

    if(t->nodetype == STRING_CONSTANT_NODE) {
        int reg = getReg();
        fprintf(target_file, "MOV R%d, %s\n", reg, t->varname);
        return reg;
    }

    if(t->nodetype == NOT_NODE) {
        int reg = codeGen(t->left, target_file);
        int zero = getReg();
        fprintf(target_file, "MOV R%d, 0\n", zero);
        fprintf(target_file, "EQ R%d, R%d\n", reg, zero);
        freeReg();
        return reg;
    }

    int left_reg = codeGen(t->left, target_file);
    int right_reg = codeGen(t->right, target_file);

    switch(t->nodetype) {
        case PLUS_NODE:
            fprintf(target_file, "ADD R%d, R%d\n", left_reg, right_reg);
            freeReg(); return left_reg;
        case MINUS_NODE:
            fprintf(target_file, "SUB R%d, R%d\n", left_reg, right_reg);
            freeReg(); return left_reg;
        case MUL_NODE:
            fprintf(target_file, "MUL R%d, R%d\n", left_reg, right_reg);
            freeReg(); return left_reg;
        case DIV_NODE:
            fprintf(target_file, "DIV R%d, R%d\n", left_reg, right_reg);
            freeReg(); return left_reg;
        case GT_NODE:
            fprintf(target_file, "GT R%d, R%d\n", left_reg, right_reg);
            freeReg(); return left_reg;
        case EQ_NODE:
            fprintf(target_file, "EQ R%d, R%d\n", left_reg, right_reg);
            freeReg(); return left_reg;
        case GE_NODE:
            fprintf(target_file, "GE R%d, R%d\n", left_reg, right_reg);
            freeReg(); return left_reg;
        case LT_NODE:
            fprintf(target_file, "LT R%d, R%d\n", left_reg, right_reg);
            freeReg(); return left_reg;
        case LE_NODE:
            fprintf(target_file, "LE R%d, R%d\n", left_reg, right_reg);
            freeReg(); return left_reg;
        case NE_NODE:
            fprintf(target_file, "NE R%d, R%d\n", left_reg, right_reg);
            freeReg(); return left_reg;
        case AND_NODE:
            // booleans are 0/1: a AND b == a * b
            fprintf(target_file, "MUL R%d, R%d\n", left_reg, right_reg);
            freeReg(); return left_reg;
        case OR_NODE:
            // a OR b == (a + b) > 0
            fprintf(target_file, "ADD R%d, R%d\n", left_reg, right_reg);
            fprintf(target_file, "MOV R%d, 0\n", right_reg);
            fprintf(target_file, "GT R%d, R%d\n", left_reg, right_reg);
            freeReg(); return left_reg;
        case MODULUS_NODE: {
            // a % b is a-(a/b)*b
            int temp_reg = getReg();
            fprintf(target_file, "MOV R%d, R%d\n", temp_reg, left_reg);
            fprintf(target_file, "DIV R%d, R%d\n", temp_reg, right_reg);
            fprintf(target_file, "MUL R%d, R%d\n", temp_reg, right_reg);
            fprintf(target_file, "SUB R%d, R%d\n", left_reg, temp_reg);
            freeReg();
            freeReg();
            return left_reg;
        }
    }
    return -1;
}

/* Header + code that sets up the stack, calls main and exits.
   Emitted once the global declarations are processed. */
void genStartup(FILE* fp) {
    fprintf(fp, "0\n2056\n0\n0\n0\n0\n0\n0\n");
    fprintf(fp, "MOV SP, %d\n", get_stack_pointer());
    fprintf(fp, "MOV BP, SP\n");
    fprintf(fp, "PUSH R0\n");          // space for main's return value
    fprintf(fp, "CALL MAIN\n");
    fprintf(fp, "POP R0\n");
    fprintf(fp, "MOV R0, \"Exit\"\n");
    fprintf(fp, "PUSH R0\n");
    fprintf(fp, "PUSH R0\n");
    fprintf(fp, "PUSH R0\n");
    fprintf(fp, "PUSH R0\n");
    fprintf(fp, "PUSH R0\n");
    fprintf(fp, "CALL 0\n");
}

/* Callee side of the calling convention */
void genFunction(char* label, int nlocals, tnode* body, FILE* fp) {
    int i;
    reg_index = -1;
    loop_top = -1;

    fprintf(fp, "%s:\n", label);
    fprintf(fp, "PUSH BP\n");
    fprintf(fp, "MOV BP, SP\n");
    for(i = 0; i < nlocals; i++) {
        fprintf(fp, "PUSH R0\n");      // space for local variables
    }

    codeGen(body, fp);

    // in case control reaches the end of the function without a return
    genReturnSequence(fp);
}
