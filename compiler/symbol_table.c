#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol_table.h"

/* ---------------- Tuple type table ---------------- */

Typetable* thead = NULL;
Typetable* ttail = NULL;
int tuple_count = 0;

Typetable* TLookup(char* name) {
    Typetable* temp = thead;
    while(temp != NULL && strcmp(temp->name, name) != 0) {
        temp = temp->next;
    }
    return temp;
}

Typetable* TLookupByCode(int typecode) {
    Typetable* temp = thead;
    while(temp != NULL && temp->typecode != typecode) {
        temp = temp->next;
    }
    return temp;
}

Fieldlist* FLookup(Typetable* t, char* fname) {
    Fieldlist* f = t->fields;
    while(f != NULL && strcmp(f->name, fname) != 0) {
        f = f->next;
    }
    return f;
}

int typeSize(int type) {
    if(isTupleType(type)) {
        Typetable* t = TLookupByCode(type);
        return t ? t->size : 1;
    }
    return 1;
}

int TInstall(char* name, Paramstruct* fields) {
    if(TLookup(name) != NULL) return -1;

    Typetable* t = (Typetable*) malloc(sizeof(Typetable));
    t->name = strdup(name);
    t->typecode = TUPLE_BASE + tuple_count++;
    t->size = 0;
    t->fields = NULL;
    t->next = NULL;

    Fieldlist* last = NULL;
    while(fields != NULL) {
        Fieldlist* scan = t->fields;
        while(scan != NULL) {
            if(strcmp(scan->name, fields->name) == 0) return -2;
            scan = scan->next;
        }
        Fieldlist* f = (Fieldlist*) malloc(sizeof(Fieldlist));
        f->name = strdup(fields->name);
        f->type = fields->type;
        f->offset = t->size;
        f->next = NULL;
        t->size += typeSize(fields->type);   // nested tuples are laid out inline
        if(last == NULL) t->fields = f; else last->next = f;
        last = f;
        fields = fields->next;
    }

    if(thead == NULL) thead = t; else ttail->next = t;
    ttail = t;
    return t->typecode;
}

int maxTupleSize() {
    int m = 0;
    Typetable* t = thead;
    while(t != NULL) {
        if(t->size > m) m = t->size;
        t = t->next;
    }
    return m;
}

void print_type_table() {
    Typetable* t = thead;
    if(t == NULL) return;
    printf("---- Tuple Type Table ----\n");
    while(t != NULL) {
        printf("tuple %s (code %d, size %d):", t->name, t->typecode, t->size);
        Fieldlist* f = t->fields;
        while(f != NULL) {
            printf(" %s[type %d, offset %d]", f->name, f->type, f->offset);
            f = f->next;
        }
        printf("\n");
        t = t->next;
    }
}

/* ---------------- Global symbol table ---------------- */

Gsymbol* head = NULL;
Gsymbol* tail = NULL;
int bind_start = 4096;
int flabel_count = 0;

Gsymbol* Lookup(char* name) {
    Gsymbol* temp = head;
    while(temp != NULL && strcmp(temp->name, name) != 0) {
        temp = temp->next;
    }
    return temp;
}

Gsymbol* get_global_head() {
    return head;
}

static void appendGlobal(Gsymbol* newentry) {
    if(head == NULL) {
        head = newentry;
        tail = head;
    } else {
        tail->next = newentry;
        tail = newentry;
    }
}

int Install(char* name, int type, int size, int* metadata) {
    if(Lookup(name) != NULL) {
        return 1;
    }

    Gsymbol* newentry = (Gsymbol*) malloc(sizeof(Gsymbol));
    newentry->name = strdup(name);
    newentry->size = size;
    newentry->type = type;
    newentry->binding = bind_start;
    bind_start += size;
    newentry->metadata = metadata;
    newentry->paramlist = NULL;
    newentry->flabel = -1;
    newentry->isFunction = 0;
    newentry->defined = 0;
    newentry->next = NULL;

    appendGlobal(newentry);
    return 0;
}

int InstallFunction(char* name, int type, Paramstruct* paramlist) {
    if(Lookup(name) != NULL) {
        return 1;
    }

    Gsymbol* newentry = (Gsymbol*) malloc(sizeof(Gsymbol));
    newentry->name = strdup(name);
    newentry->size = 0;
    newentry->type = type;
    newentry->binding = -1;
    newentry->metadata = NULL;
    newentry->paramlist = paramlist;
    newentry->flabel = flabel_count++;
    newentry->isFunction = 1;
    newentry->defined = 0;
    newentry->next = NULL;

    appendGlobal(newentry);
    return 0;
}

void print_symbol_table() {
    Gsymbol* temp = head;
    printf("---- Global Symbol Table ----\n");
    printf("%-15s %-7s %-7s %-8s %-7s\n", "name", "type", "size", "binding", "flabel");
    while(temp != NULL) {
        if(temp->isFunction) {
            printf("%-15s %-7d %-7d %-8s F%-6d (", temp->name, temp->type, temp->size, "-", temp->flabel);
            Paramstruct* p = temp->paramlist;
            while(p != NULL) {
                printf("%d %s%s", p->type, p->name, p->next ? ", " : "");
                p = p->next;
            }
            printf(")\n");
        } else {
            printf("%-15s %-7d %-7d %-8d %-7s\n", temp->name, temp->type, temp->size, temp->binding, "-");
        }
        temp = temp->next;
    }
}

/* Address of the last word used by global variables (initial SP) */
int get_stack_pointer() {
    return bind_start - 1;
}

/* ---------------- Parameter lists ---------------- */

Paramstruct* makeParam(char* name, int type) {
    Paramstruct* p = (Paramstruct*) malloc(sizeof(Paramstruct));
    p->name = strdup(name);
    p->type = type;
    p->next = NULL;
    return p;
}

Paramstruct* appendParam(Paramstruct* list, Paramstruct* p) {
    if(list == NULL) return p;
    Paramstruct* temp = list;
    while(temp->next != NULL) temp = temp->next;
    temp->next = p;
    return list;
}

int countParams(Paramstruct* list) {
    int n = 0;
    while(list != NULL) {
        n++;
        list = list->next;
    }
    return n;
}

/* ---------------- Local symbol table ---------------- */

Lsymbol* Lhead = NULL;
Lsymbol* Ltail = NULL;
int local_count = 0;

void LReset() {
    Lhead = NULL;
    Ltail = NULL;
    local_count = 0;
}

Lsymbol* LLookup(char* name) {
    Lsymbol* temp = Lhead;
    while(temp != NULL && strcmp(temp->name, name) != 0) {
        temp = temp->next;
    }
    return temp;
}

static int LAppend(char* name, int type, int binding) {
    if(LLookup(name) != NULL) {
        return 1;
    }
    Lsymbol* newentry = (Lsymbol*) malloc(sizeof(Lsymbol));
    newentry->name = strdup(name);
    newentry->type = type;
    newentry->binding = binding;
    newentry->next = NULL;

    if(Lhead == NULL) {
        Lhead = newentry;
        Ltail = newentry;
    } else {
        Ltail->next = newentry;
        Ltail = newentry;
    }
    return 0;
}

/* Stack below BP (stack grows upward):
     [BP-1]                        return address
     [BP-retsize-1 .. BP-2]        return value (retsize words; 1 for scalars)
     then parameter 1, parameter 2, ...  each occupying typeSize words.
   The binding of a parameter is the (lowest) address of its first word, relative to BP.
   For scalar functions this gives param1 at BP-3, param2 at BP-4, ... */
int LInstallParams(Paramstruct* params, int retsize) {
    int cum = 0;
    while(params != NULL) {
        cum += typeSize(params->type);
        if(LAppend(params->name, params->type, -(1 + retsize + cum))) {
            return 1;
        }
        params = params->next;
    }
    return 0;
}

/* Locals are laid out from BP+1 upward; a tuple local takes typeSize words */
int LInstall(char* name, int type, int size) {
    if(LAppend(name, type, local_count + 1)) {
        return 1;
    }
    local_count += size;
    return 0;
}

int LLocalCount() {
    return local_count;
}

void print_local_table(char* fname) {
    Lsymbol* temp = Lhead;
    printf("---- Local Symbol Table: %s ----\n", fname);
    printf("%-15s %-7s %-7s\n", "name", "type", "binding");
    while(temp != NULL) {
        printf("%-15s %-7d BP%+d\n", temp->name, temp->type, temp->binding);
        temp = temp->next;
    }
}
