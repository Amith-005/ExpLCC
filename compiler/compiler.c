#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"

#define MAX_REGS 20
static int reg_used[MAX_REGS];

void initializeRegs(void) {
    for (int i = 0; i < MAX_REGS; i++) {
        reg_used[i] = 0;
    }
}

int getReg(void) {
    for (int i = 0; i < MAX_REGS; i++) {
        if (reg_used[i] == 0) {
            reg_used[i] = 1;
            return i;
        }
    }
    fprintf(stderr, "Out of registers\n");
    exit(1);
}

void freeReg(void) {
    for (int i = MAX_REGS - 1; i >= 0; i--) {
        if (reg_used[i] == 1) {
            reg_used[i] = 0;
            return;
        }
    }
}

tnode* makeLeafNode(int n) {
    tnode *temp = (tnode *)malloc(sizeof(tnode));
    if (!temp) {
        perror("malloc");
        exit(1);
    }
    temp->val = n;
    temp->op = '\0';
    temp->left = NULL;
    temp->right = NULL;
    return temp;
}

tnode* makeOperatorNode(char c, tnode *l, tnode *r) {
    tnode *temp = (tnode *)malloc(sizeof(tnode));
    if (!temp) {
        perror("malloc");
        exit(1);
    }
    temp->op = c;
    temp->val = 0;
    temp->left = l;
    temp->right = r;
    return temp;
}

int codeGen(tnode *t, FILE *target_file) {
    if (t->left == NULL && t->right == NULL) {
        int reg = getReg();
        fprintf(target_file, "MOV R%d, %d\n", reg, t->val);
        return reg;
    }

    int leftReg = codeGen(t->left, target_file);
    int rightReg = codeGen(t->right, target_file);

    switch (t->op) {
        case '+': fprintf(target_file, "ADD R%d, R%d\n", leftReg, rightReg); break;
        case '-': fprintf(target_file, "SUB R%d, R%d\n", leftReg, rightReg); break;
        case '*': fprintf(target_file, "MUL R%d, R%d\n", leftReg, rightReg); break;
        case '/': fprintf(target_file, "DIV R%d, R%d\n", leftReg, rightReg); break;
        default:
            fprintf(stderr, "Unknown operator %c\n", t->op);
            exit(1);
    }

    freeReg();
    return leftReg;
}
