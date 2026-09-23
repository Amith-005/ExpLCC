#ifndef EX2_H
#define EX2_H

#include <stdlib.h>
#include <stdio.h>


typedef enum Nodetype
{
    VARIABLE,
    CONSTANT,
    READ,
    WRITE,
    STATEMENT,
    OPERATOR,
    WHILE,
    IF,
    BREAK,
    CONTINUE,
    REPEAT,
    DOWHILE,
    ARRAY,
    ADDRESS,
    POINTER, 
} Nodetype;

typedef enum Type
{
    INTEGER,
    BOOLEAN,
    VOID,
    STRING,
    POINTER_TO_INTEGER, 
    POINTER_TO_STRING,  
} Type;

typedef struct GST_Node
{
    char *name;
    int type;
    int size;
    int size2;
    int binding;
    int dimensions;
    struct GST_Node *next;
    int ptr_type;
} GST_Node;


typedef struct AST_Node
{
    int val;
    Type type;
    char *varname;
    Nodetype nodetype;
    struct GST_Node *GSTentry;
    char *s;
    struct AST_Node *left, *mid, *right;
} AST_Node;


struct AST_Node *makeConstantLeafNode(Type, int, char *);
struct AST_Node *makeVariableLeafNode(char *, char *);
struct AST_Node *makeNode(Nodetype, Type, struct AST_Node *, struct AST_Node *, struct AST_Node *, char *);
struct AST_Node *makeArrayLeafNode(char *, struct AST_Node *, char *);
struct AST_Node *makeArray2DLeafNode(char *varname, struct AST_Node *row, struct AST_Node *col, char *s);
struct AST_Node *makePointerNode(Nodetype node_type, struct AST_Node *l, char *s); 

struct GST_Node *GSTLookup(char *);
struct GST_Node *GSTInstall(char *, Type, int size, int size2, int dimensions, int ptr_type); 
void GSTChangeType(struct AST_Node *, Type);
struct AST_Node *ASTChangeType(struct AST_Node *root, Type type);
void GSTPrint();

void loopStackPush(int start, int end);
void loopStackPop();
int loopStackTopBreak();
int loopStackTopContinue();

void printIndent(int, int);
void print_tree(struct AST_Node *, int, int);
int getReg();
void freeReg();
int getLabel();
int getAddr(struct AST_Node *t);
int codeGen(struct AST_Node *, FILE *);
void xsmgenerator(struct AST_Node *t);

#endif
