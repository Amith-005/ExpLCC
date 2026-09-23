#include "compiler.h"
#include <string.h>
#include <stdlib.h>


GST_Node *Ghead = NULL;
int binding = 4096;
int SP;

typedef struct LoopNode {
    int startLabel;
    int endLabel;
    struct LoopNode *next;
} LoopNode;

LoopNode *loopStack = NULL;

void loopStackPush(int start, int end) {
    LoopNode *new_node = (LoopNode *)malloc(sizeof(LoopNode));
    new_node->startLabel = start;
    new_node->endLabel = end;
    new_node->next = loopStack;
    loopStack = new_node;
}

void loopStackPop() {
    if (loopStack != NULL) {
        LoopNode *temp = loopStack;
        loopStack = loopStack->next;
        free(temp);
    }
}

int loopStackTopBreak() {
    if (loopStack != NULL) {
        return loopStack->endLabel;
    }
    printf("Error: BREAK statement outside loop.\n");
    exit(1);
    return -1;
}

int loopStackTopContinue() {
    if (loopStack != NULL) {
        return loopStack->startLabel;
    }
    printf("Error: CONTINUE statement outside loop.\n");
    exit(1);
    return -1;
}
// -------------------------------------------------------------

GST_Node *GSTLookup(char *name)
{
    GST_Node *temp = Ghead;
    while (temp != NULL)
    {
        if (strcmp(temp->name, name) == 0)
            return temp;
        temp = temp->next;
    }
    return NULL;
}

GST_Node *GSTInstall(char *name, Type type, int size1, int size2, int dimensions, int ptr_type)
{
    if(GSTLookup(name)!=NULL){
        printf("Var redeclared\n");
        printf("The redeclared var is:%s\n",name);
        exit(1);
    }
    GST_Node *new_node = (GST_Node *)malloc(sizeof(GST_Node));
    new_node->name = strdup(name);
    
    // Set type based on whether it is a pointer or not
    if (ptr_type == INTEGER && type == VOID) {
        new_node->type = POINTER_TO_INTEGER;
        new_node->ptr_type = INTEGER;
    } else if (ptr_type == STRING && type == VOID) {
        new_node->type = POINTER_TO_STRING;
        new_node->ptr_type = STRING;
    } else {
        new_node->type = type;
        new_node->ptr_type = type;
    }

    new_node->size = size1;
    new_node->size2 = size2;
    new_node->dimensions = dimensions;
    new_node->binding = binding;

    int total_size = size1 * size2;
    if (dimensions == 1) total_size = size1;
    if (dimensions == 0) total_size = 1;

    binding += total_size;
    new_node->next = NULL;

    if (Ghead == NULL) {
        Ghead = new_node;
    } else {
        GST_Node *temp = Ghead;
        while (temp->next != NULL)
            temp = temp->next;
        temp->next = new_node;
    }
    SP = binding;
    return new_node;
}

void GSTChangeType(AST_Node *root, Type type)
{
    if (root != NULL) {
        if (root->nodetype == VARIABLE) {
            GST_Node *temp = GSTLookup(root->varname);
            if (temp != NULL) {
                if (type == INTEGER && temp->type == VOID) {
                    temp->type = POINTER_TO_INTEGER;
                    temp->ptr_type = INTEGER;
                } else if (type == STRING && temp->type == VOID) {
                    temp->type = POINTER_TO_STRING;
                    temp->ptr_type = STRING;
                } else {
                    temp->type = type;
                    temp->ptr_type = type;
                }
                root->type = temp->type;
            }
        }
        GSTChangeType(root->left, type);
        GSTChangeType(root->right, type);
    }
}

void GSTPrint()
{
    char *type_str;
    char *array_str;
    struct GST_Node *temp = Ghead;
    printf("Name\tType\tSize\tArray\tBinding\n");
    while (temp != NULL)
    {
        type_str = (char *)malloc(sizeof(char) * 12);
        array_str = (char *)malloc(sizeof(char) * 4);

        if (temp->type == INTEGER) strcpy(type_str, "int");
        else if (temp->type == STRING) strcpy(type_str, "str");
        else if (temp->type == POINTER_TO_INTEGER) strcpy(type_str, "int *");
        else if (temp->type == POINTER_TO_STRING) strcpy(type_str, "str *");
        else strcpy(type_str, "void");


        if (temp->dimensions == 0) {
            strcpy(array_str, "no");
        }
        else if (temp->dimensions == 1 || temp->dimensions == 2) {
            strcpy(array_str, "yes");
        }

        int size_to_print = (temp->size2 != 0) ? (temp->size * temp->size2) : temp->size;

        printf("%s\t%s\t%d\t%s\t%d\n", temp->name, type_str, size_to_print, array_str, temp->binding);
            
        free(type_str);
        free(array_str);
        temp = temp->next;
    }
}

int getSP()
{
    return SP;
}

AST_Node *makeVariableLeafNode(char *varname, char *s)
{
    AST_Node *new_node = (AST_Node *)malloc(sizeof(AST_Node));
    new_node->s = strdup(s);
    new_node->nodetype = VARIABLE;
    new_node->varname = strdup(varname);
    new_node->GSTentry = GSTLookup(varname);
    if (new_node->GSTentry)
        new_node->type = new_node->GSTentry->type;
    else
        new_node->type = VOID; 
    new_node->left = NULL;
    new_node->right = NULL;
    new_node->mid = NULL;
    return new_node;
}

AST_Node *makeConstantLeafNode(Type type, int val, char *s)
{
    AST_Node *new_node = (AST_Node *)malloc(sizeof(AST_Node));
    new_node->s = strdup(s);
    new_node->nodetype = CONSTANT;
    new_node->type = type;
    new_node->val = val;
    new_node->GSTentry = NULL;
    new_node->left = NULL;
    new_node->right = NULL;
    new_node->mid = NULL;
    new_node->varname = NULL;
    return new_node;
}

struct AST_Node *makeArrayLeafNode(char *varname, struct AST_Node *l, char *s)
{
    struct AST_Node *new_node = (struct AST_Node *)malloc(sizeof(struct AST_Node));
    new_node->s = strdup(s);
    new_node->nodetype = ARRAY;
    new_node->varname = strdup(varname);
    new_node->GSTentry = GSTLookup(varname);
    if (new_node->GSTentry)
        new_node->type = new_node->GSTentry->type;
    else {
        printf("Error: Array name '%s' not found.\n", varname);
        exit(1);
    }
    new_node->left = l;
    new_node->right = (struct AST_Node *)NULL;
    new_node->mid = (struct AST_Node *)NULL;
    return new_node;
}

struct AST_Node *makeArray2DLeafNode(char *varname, struct AST_Node *row, struct AST_Node *col, char *s)
{
    struct AST_Node *new_node = (struct AST_Node *)malloc(sizeof(struct AST_Node));
    new_node->s = strdup(s);
    new_node->nodetype = ARRAY;
    new_node->varname = strdup(varname);
    new_node->GSTentry = GSTLookup(varname);
    if (new_node->GSTentry)
        new_node->type = new_node->GSTentry->type;
    else {
        printf("Error: 2D Array name '%s' not found.\n", varname);
        exit(1);
    }
    new_node->left = row; 
    new_node->mid = col;
    new_node->right = NULL;
    return new_node;
}

struct AST_Node *makePointerNode(Nodetype node_type, struct AST_Node *l, char *s)
{
    struct AST_Node *new_node = (struct AST_Node *)malloc(sizeof(struct AST_Node));
    new_node->s = strdup(s);
    new_node->nodetype = node_type;
    new_node->left = l;
    new_node->mid = NULL;
    new_node->right = NULL;
    new_node->GSTentry = NULL;
    new_node->varname = NULL;

    if (node_type == ADDRESS) { 
        if (l->nodetype != VARIABLE && l->nodetype != ARRAY) {
            printf("Error: Address-of operator '&' can only be applied to variables or arrays.\n");
            exit(1);
        }
        
        switch (l->type) {
            case INTEGER:
                new_node->type = POINTER_TO_INTEGER;
                break;
            case STRING:
                new_node->type = POINTER_TO_STRING;
                break;
            case POINTER_TO_INTEGER: 
                new_node->type = POINTER_TO_INTEGER;
                break;
            default:
                printf("Error: Invalid type for address-of operator (Type %d).\n", l->type);
                exit(1);
        }
    } else if (node_type == POINTER) { // *p
        // Dereferencing a pointer gives the type it points to.
        if (l->type == POINTER_TO_INTEGER) new_node->type = INTEGER;
        else if (l->type == POINTER_TO_STRING) new_node->type = STRING;
        else {
            printf("Error: Dereference operator '*' can only be applied to a pointer variable (Got Type %d).\n", l->type);
            exit(1);
        }
    }
    return new_node;
}


AST_Node *makeNode(Nodetype node_type, Type type, AST_Node *l, AST_Node *m, AST_Node *r, char *s)
{
    AST_Node *new_node = (AST_Node *)malloc(sizeof(AST_Node));
    
    if (node_type == OPERATOR) {
        if (strcmp(s, "=") == 0) {
            if (l->nodetype != VARIABLE && l->nodetype != ARRAY && l->nodetype != POINTER) {
                printf("Error: Left side of assignment must be an assignable variable, array element, or dereferenced pointer.\n");
                exit(1);
            }

            int assignment_allowed = 0;
            int r_is_array_name = (r->nodetype == VARIABLE && r->GSTentry && r->GSTentry->dimensions > 0);
            
            if (l->type == POINTER_TO_INTEGER && r_is_array_name && r->GSTentry->ptr_type == INTEGER) {
                assignment_allowed = 1; 
            }
            else if (l->type == POINTER_TO_STRING && r_is_array_name && r->GSTentry->ptr_type == STRING) {
                assignment_allowed = 1; 
            }
            else if (r->nodetype == ADDRESS) {
                assignment_allowed = 1; 
            }


            if (!assignment_allowed) {
                Type l_base_type = l->type;
                Type r_base_type = r->type;

                if (l_base_type == POINTER_TO_INTEGER || l_base_type == POINTER_TO_STRING) {
                    l_base_type = INTEGER; 
                }
                if (r_base_type == POINTER_TO_INTEGER || r_base_type == POINTER_TO_STRING) {
                    r_base_type = INTEGER;
                }

                if (l_base_type != r_base_type) { 
                     printf("Error: Type mismatch in assignment. Expected type %d, got %d.\n", l->type, r->type);
                     exit(1);
                }
            }
        }
        else if (l->type == POINTER_TO_INTEGER || r->type == POINTER_TO_INTEGER) {
            if (strcmp(s, "+") == 0 || strcmp(s, "-") == 0) {
                if (!((l->type == POINTER_TO_INTEGER && r->type == INTEGER) || (l->type == INTEGER && r->type == POINTER_TO_INTEGER))) {
                     printf("Error: Invalid pointer arithmetic. Only pointer +/- integer is allowed.\n");
                     exit(1);
                }
                
                if (l->type == POINTER_TO_INTEGER || r->type == POINTER_TO_INTEGER) {
                    type = POINTER_TO_INTEGER;
                }
                
            } else {
                 printf("Error: Invalid operator '%s' for pointer or address types.\n", s);
                 exit(1);
            }
        }
        else if (type == INTEGER) {
            if (l->type != INTEGER || r->type != INTEGER) {
                printf("Error: Type mismatch. Arithmetic operator requires integer operands.\n");
                exit(1);
            }
        }
        else if (type == BOOLEAN) {
            if (l->type != r->type) {
                printf("Error: Type mismatch. Relational operator requires compatible operands.\n");
                exit(1);
            }
        }
    }

    if (node_type == WHILE || node_type == IF || node_type == REPEAT || node_type == DOWHILE) {
        AST_Node *condition = (node_type == IF) ? l : r; 
        if (node_type == WHILE || node_type == IF) condition = l;
        else if (node_type == REPEAT || node_type == DOWHILE) condition = r; 

        if (condition->type != BOOLEAN) {
            printf("Error: Type mismatch. Condition for loop/IF must be BOOLEAN.\n");
            exit(1);
        }
    }

    new_node->s = strdup(s);
    new_node->nodetype = node_type;
    new_node->type = type;
    new_node->left = l;
    new_node->mid = m;
    new_node->right = r;
    new_node->GSTentry = NULL;
    new_node->varname = NULL;
    return new_node;
}

struct AST_Node *ASTChangeType(struct AST_Node *root, Type type)
{
    if (root != NULL)
    {
        if(root->left) root->left = ASTChangeType(root->left, type);
        if(root->right) root->right = ASTChangeType(root->right, type);

        if (root->nodetype == VARIABLE)
        {
            GSTChangeType(root, type);
        }
    }
    return root;
}

void printIndent(int depth, int isRight) {
    for (int i = 0; i < depth - 1; i++) {
        printf("|   ");
    }
    if (depth > 0) {
        printf(isRight ? "└─ " : "├─ ");
    }
}

void print_tree(AST_Node *root, int lvl, int isRight)
{
    if (root == NULL) return;
    printIndent(lvl, isRight);
    printf("%s", root->s);

    if (root->nodetype == VARIABLE && root->GSTentry) {
        printf(" (Binding: %d)", root->GSTentry->binding);
    }
    printf("\n");

    if (root->left != NULL)
        print_tree(root->left, lvl + 1, 0);
    if (root->mid != NULL)
        print_tree(root->mid, lvl + 1, 1);
    if (root->right != NULL)
        print_tree(root->right, lvl + 1, 1);
}


int free_reg = -1;
int label = 0;

int getReg() {
    if (free_reg >= 19) { 
        printf("Error: Out of registers.\n");
        exit(1);
    }
    free_reg++;
    return free_reg;
}

void freeReg() {
    if (free_reg >= 0) {
        free_reg--;
    }
}

int getLabel() {
    return label++;
}

int getAddr(AST_Node *t) {
    if (t->GSTentry != NULL) {
        return t->GSTentry->binding;
    }
    return -1;
}

int codeGen(AST_Node *t, FILE *target_file)
{
    int p, q, r, s, addr;

    if (t == NULL) return -1;

    if (t->nodetype == CONSTANT) {
        p = getReg();
        if (t->type == INTEGER) {
            fprintf(target_file, "MOV R%d, %d\n", p, t->val);
        } else if (t->type == STRING) {
            fprintf(target_file, "MOV R%d, %s\n", p, t->s);
        }
        return p;
    }
    
    if (t->nodetype == ADDRESS) { 
        p = getReg();
        if (t->left->nodetype == VARIABLE) {
            addr = getAddr(t->left);
            fprintf(target_file, "MOV R%d, %d\n", p, addr);
        } else if (t->left->nodetype == ARRAY) {
            if (t->left->GSTentry->dimensions == 1) { // 1D array
                addr = getAddr(t->left);
                q = codeGen(t->left->left, target_file); // Index in Rq
                fprintf(target_file, "MOV R%d, %d\n", p, addr); // Base address in Rp
                fprintf(target_file, "ADD R%d, R%d\n", p, q); // Rp = Base + Index
                freeReg();
            } else if (t->left->GSTentry->dimensions == 2) { 
                 printf("Error: Address-of not fully supported for 2D array elements.\n");
                 exit(1);
            }
        }
        return p;
    }

    if (t->nodetype == VARIABLE) {
        p = getReg();
        addr = getAddr(t);

        if (t->GSTentry && t->GSTentry->dimensions > 0) {
            fprintf(target_file, "MOV R%d, %d\n", p, addr);
            return p;
        }

        if (t->type == POINTER_TO_INTEGER || t->type == POINTER_TO_STRING) {
            fprintf(target_file, "MOV R%d, [%d]\n", p, addr); 
        } else {
            fprintf(target_file, "MOV R%d, [%d]\n", p, addr); 
        }
        return p;
    }
    
    if (t->nodetype == POINTER) { 
        p = codeGen(t->left, target_file); 
        fprintf(target_file, "MOV R%d, [R%d]\n", p, p);
        return p;
    }

    if (t->nodetype == READ) {
        int addr_reg = getReg();
        int q = getReg();
        
        if (t->left->nodetype == ARRAY) {
             addr = getAddr(t->left);
             if (t->left->GSTentry->dimensions == 2) {
                 int row_reg = codeGen(t->left->left, target_file);
                 int col_reg = codeGen(t->left->mid, target_file);
                 int dim_reg = getReg();
                 fprintf(target_file, "MOV R%d, %d\n", dim_reg, t->left->GSTentry->size2);
                 fprintf(target_file, "MUL R%d, R%d\n", row_reg, dim_reg);
                 fprintf(target_file, "ADD R%d, R%d\n", row_reg, col_reg);
                 fprintf(target_file, "MOV R%d, %d\n", addr_reg, addr);
                 fprintf(target_file, "ADD R%d, R%d\n", addr_reg, row_reg);
                 freeReg(); freeReg(); freeReg();
             } else { // 1D array
                 int index_reg = codeGen(t->left->left, target_file);
                 fprintf(target_file, "MOV R%d, %d\n", addr_reg, addr);
                 fprintf(target_file, "ADD R%d, R%d\n", addr_reg, index_reg);
                 freeReg();
             }
        } else if (t->left->nodetype == POINTER) {
            addr_reg = codeGen(t->left->left, target_file);
        } else { // Simple variable
             addr = getAddr(t->left);
             fprintf(target_file, "MOV R%d, %d\n", addr_reg, addr);
        }

        fprintf(target_file, "MOV R%d, \"Read\"\n", q);
        fprintf(target_file, "PUSH R%d\n", q);
        fprintf(target_file, "MOV R%d, -1\n", q);
        fprintf(target_file, "PUSH R%d\n", q);
        fprintf(target_file, "PUSH R%d\n", addr_reg); 
        fprintf(target_file, "PUSH R%d\n", q);
        fprintf(target_file, "PUSH R%d\n", q);
        fprintf(target_file, "CALL 0\n");
        fprintf(target_file, "POP R%d\nPOP R%d\nPOP R%d\nPOP R%d\nPOP R%d\n", q,q,q,q,q);
        freeReg(); freeReg();
        return -1;
    }

    if (t->nodetype == WRITE) {
        p = codeGen(t->left, target_file);
        q = getReg();
        
        fprintf(target_file, "MOV R%d, \"Write\"\n", q);
        fprintf(target_file, "PUSH R%d\n", q);
        fprintf(target_file, "MOV R%d, -2\n", q);
        fprintf(target_file, "PUSH R%d\n", q);
        fprintf(target_file, "PUSH R%d\n", p);
        fprintf(target_file, "PUSH R%d\n", q);
        fprintf(target_file, "PUSH R%d\n", q);
        fprintf(target_file, "CALL 0\n");
        fprintf(target_file, "POP R%d\nPOP R%d\nPOP R%d\nPOP R%d\nPOP R%d\n", q,q,q,q,q);
        freeReg(); 
        freeReg();
        return -1;
    }

    if (t->nodetype == STATEMENT) {
        codeGen(t->left, target_file);
        codeGen(t->right, target_file);
        return -1;
    }

    if (t->nodetype == OPERATOR) {
        if (t->type == BOOLEAN) {
            p = codeGen(t->left, target_file);
            q = codeGen(t->right, target_file);
            if (strcmp(t->s, ">") == 0) fprintf(target_file, "GT R%d, R%d\n", p, q);
            else if (strcmp(t->s, "<") == 0) fprintf(target_file, "LT R%d, R%d\n", p, q);
            else if (strcmp(t->s, ">=") == 0) fprintf(target_file, "GE R%d, R%d\n", p, q);
            else if (strcmp(t->s, "<=") == 0) fprintf(target_file, "LE R%d, R%d\n", p, q);
            else if (strcmp(t->s, "==") == 0) fprintf(target_file, "EQ R%d, R%d\n", p, q);
            else if (strcmp(t->s, "!=") == 0) fprintf(target_file, "NE R%d, R%d\n", p, q);
            freeReg();
            return p;
        }

        if (strcmp(t->s, "=") == 0) {  
            int rhs = codeGen(t->right, target_file);
            
            if (t->left->nodetype == ARRAY) {
                addr = getAddr(t->left);
                p = getReg();
                if (t->left->GSTentry->dimensions == 2) {
                    int row_reg = codeGen(t->left->left, target_file);
                    int col_reg = codeGen(t->left->mid, target_file);
                    int dim_reg = getReg();
                    fprintf(target_file, "MOV R%d, %d\n", dim_reg, t->left->GSTentry->size2);
                    fprintf(target_file, "MUL R%d, R%d\n", row_reg, dim_reg);
                    fprintf(target_file, "ADD R%d, R%d\n", row_reg, col_reg);
                    fprintf(target_file, "MOV R%d, %d\n", p, addr);
                    fprintf(target_file, "ADD R%d, R%d\n", p, row_reg); 
                    freeReg(); freeReg(); freeReg();
                } else { // 1D array
                    int index_reg = codeGen(t->left->left, target_file);
                    fprintf(target_file, "MOV R%d, %d\n", p, addr);
                    fprintf(target_file, "ADD R%d, R%d\n", p, index_reg); 
                    freeReg();
                }
                fprintf(target_file, "MOV [R%d], R%d\n", p, rhs);
                freeReg();
                freeReg();
            } else if (t->left->nodetype == POINTER) { 
                p = codeGen(t->left->left, target_file); 
                fprintf(target_file, "MOV [R%d], R%d\n", p, rhs); 
                freeReg();
                freeReg();
            } else if (t->left->nodetype == VARIABLE) {
                 addr = getAddr(t->left);
                 fprintf(target_file, "MOV [%d], R%d\n", addr, rhs);
                 freeReg();
            } else {
                 printf("Error: Invalid L-value in assignment.\n");
                 exit(1);
            }
            return -1;
        } else {
            p = codeGen(t->left, target_file);
            q = codeGen(t->right, target_file);

            if (t->left->type == POINTER_TO_INTEGER || t->right->type == POINTER_TO_INTEGER) {
                if (t->left->type == POINTER_TO_INTEGER) { 
                    if (strcmp(t->s, "+") == 0) fprintf(target_file, "ADD R%d, R%d\n", p, q);
                    else if (strcmp(t->s, "-") == 0) fprintf(target_file, "SUB R%d, R%d\n", p, q);
                } else { // I + P (I is Rp, P is Rq)
                    if (strcmp(t->s, "+") == 0) fprintf(target_file, "ADD R%d, R%d\n", q, p);
                    else {
                         printf("Error: Cannot subtract a pointer from an integer.\n");
                         exit(1);
                    }
                    p = q; 
                }
            } else { // Standard Arithmetic
                switch (t->s[0]) {
                    case '+': fprintf(target_file, "ADD R%d, R%d\n", p, q); break;
                    case '-': fprintf(target_file, "SUB R%d, R%d\n", p, q); break;
                    case '*': fprintf(target_file, "MUL R%d, R%d\n", p, q); break;
                    case '/': fprintf(target_file, "DIV R%d, R%d\n", p, q); break;
                }
            }
            freeReg();
            return p;
        }
    }

    if (t->nodetype == WHILE) {
        int start = getLabel(), end = getLabel();
        loopStackPush(start, end);
        fprintf(target_file, "L%d:\n", start);
        p = codeGen(t->left, target_file);
        fprintf(target_file, "JZ R%d, L%d\n", p, end);
        freeReg();
        codeGen(t->right, target_file);
        fprintf(target_file, "JMP L%d\n", start);
        fprintf(target_file, "L%d:\n", end);
        loopStackPop();
        return -1;
    }

    if (t->nodetype == DOWHILE) {
        int start = getLabel(), end = getLabel();
        loopStackPush(start, end);
        fprintf(target_file, "L%d:\n", start);
        codeGen(t->left, target_file);
        p = codeGen(t->right, target_file);
        fprintf(target_file, "JNZ R%d, L%d\n", p, start);
        freeReg();
        fprintf(target_file, "L%d:\n", end);
        loopStackPop();
        return -1;
    }

    if (t->nodetype == REPEAT) {
        int start = getLabel(), end = getLabel();
        loopStackPush(start, end);
        fprintf(target_file, "L%d:\n", start);
        codeGen(t->left, target_file);
        p = codeGen(t->right, target_file);
        fprintf(target_file, "JZ R%d, L%d\n", p, start);
        freeReg();
        fprintf(target_file, "L%d:\n", end);
        loopStackPop();
        return -1;
    }

    if (t->nodetype == IF) {
        p = codeGen(t->left, target_file);
        int l1 = getLabel();
        fprintf(target_file, "JZ R%d, L%d\n", p, l1);
        freeReg();
        codeGen(t->mid, target_file);
        if (t->right) {
            int l2 = getLabel();
            fprintf(target_file, "JMP L%d\n", l2);
            fprintf(target_file, "L%d:\n", l1);
            codeGen(t->right, target_file);
            fprintf(target_file, "L%d:\n", l2);
        } else {
            fprintf(target_file, "L%d:\n", l1);
        }
        return -1;
    }

    if (t->nodetype == BREAK) {
        fprintf(target_file, "JMP L%d\n", loopStackTopBreak());
        return -1;
    }

    if (t->nodetype == CONTINUE) {
        fprintf(target_file, "JMP L%d\n", loopStackTopContinue());
        return -1;
    }

    if (t->nodetype == ARRAY) {
        if (t->GSTentry->dimensions == 2) {
            addr = getAddr(t);
            p = codeGen(t->left, target_file); 
            q = codeGen(t->mid, target_file); 
            int dim_reg = getReg();
            int offset_reg = getReg();
            int n = t->GSTentry->size2;
            fprintf(target_file, "MOV R%d, %d\n", dim_reg, n);
            fprintf(target_file, "MUL R%d, R%d\n", p, dim_reg);
            fprintf(target_file, "ADD R%d, R%d\n", p, q); 
            fprintf(target_file, "MOV R%d, %d\n", offset_reg, addr);
            fprintf(target_file, "ADD R%d, R%d\n", offset_reg, p); 
            fprintf(target_file, "MOV R%d, [R%d]\n", p, offset_reg); 
            freeReg(); freeReg(); freeReg();
            return p;
        } else {
            addr = getAddr(t);
            p = codeGen(t->left, target_file); 
            q = getReg();
            fprintf(target_file, "MOV R%d, %d\n", q, addr);
            fprintf(target_file, "ADD R%d, R%d\n", q, p); 
            fprintf(target_file, "MOV R%d, [R%d]\n", p, q); 
            freeReg();
            return p;
        }
    }

    return -1;
}
