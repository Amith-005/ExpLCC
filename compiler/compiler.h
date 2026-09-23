#ifndef EXPTREE_H
#define EXPTREE_H
#include <stdio.h>
#include "symbol_table.h"

/* Node Type Constants */
#define PROGRAM 0
#define STATEMENT 1
#define EXPRESSION 2
#define VARIABLE 3
#define INPUT 4
#define OUTPUT 5
#define ASSIGNMENT 6
#define PLUS_NODE 7
#define MINUS_NODE 8
#define MUL_NODE 9
#define DIV_NODE 10
#define CONSTANT 11
#define READ_NODE 13
#define WRITE_NODE 14
#define EQ_NODE 15
#define NE_NODE 16
#define GT_NODE 17
#define LT_NODE 18
#define GE_NODE 19
#define LE_NODE 20
#define IF_NODE 21
#define WHILE_NODE 22
#define BREAK_NODE 23
#define CONTINUE_NODE 24
#define DO_WHILE_NODE 25
#define REPEAT_UNTIL_NODE 26
#define STRING_CONSTANT_NODE 27
#define ADDRESS_NODE 28
#define DEREF_NODE 29
#define MODULUS_NODE 30
#define FUNC_CALL_NODE 31   // varname = function name, Gentry = function entry, left = argument list
#define ARG_NODE 32         // left = argument expression, right = next ARG_NODE
#define RETURN_NODE 33      // left = returned expression
#define AND_NODE 34
#define OR_NODE 35
#define NOT_NODE 36
#define FIELD_NODE 37       // left = tuple-valued expression, val = field offset, varname = field name

/* Type constants are in symbol_table.h */


typedef struct tnode {
    int val;        // value of a number for NUM nodes.
    int type;       // type of the expression represented by this node
    char* varname;  // name of a variable / function for ID nodes
    int nodetype;   // information about non-leaf nodes - read/write/connector/+/* etc.
    Gsymbol* Gentry; // global symbol table entry (global variables and functions)
    Lsymbol* Lentry; // local symbol table entry (locals and parameters)
    struct tnode *left,*right,*mid;
} tnode;

/*Create a node tnode*/
tnode* createNode(int val, int type, int nodetype, char* c, struct tnode *l, struct tnode *r, struct tnode* m);

/* Variable reference: resolves name in local table first, then global table */
tnode* makeVarNode(char* name, tnode* index1, tnode* index2);

/* Function call node with argument list (chain of ARG_NODEs) */
tnode* makeCallNode(char* name, tnode* args);

int codeGen(tnode *t, FILE *target_file);

int getLabel();

int getReg();

void freeReg();

/* Code generation driver */
void setCurrentFunction(Gsymbol* f, int returnType);
int getCurrentReturnType();
void genStartup(FILE* fp);
void genFunction(char* label, int nlocals, tnode* body, FILE* fp);

/* Tuple field access: base must be tuple valued */
tnode* makeFieldNode(tnode* base, char* fname);

/* Static scratch area that receives tuples returned by functions */
void allocTupleScratch();

int pointerTo(int base_type);

#endif
