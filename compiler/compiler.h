#ifndef TASK2_H
#define TASK2_H

#include <stdio.h>

typedef struct tnode {
    int val;
    char op;
    struct tnode *left, *right;
} tnode;

tnode* makeLeafNode(int n);
tnode* makeOperatorNode(char c, tnode *l, tnode *r);
void initializeRegs(void);
int getReg(void);
void freeReg(void);
int codeGen(tnode *t, FILE *target_file);

#endif // TASK2_H
