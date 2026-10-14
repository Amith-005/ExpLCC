#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol_table.h"

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
    newentry->elem = 0;
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
    newentry->elem = 0;
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

/* Parameter i (1-based) is at BP-(i+2): [BP-1] = return address, [BP-2] = return value */
int LInstallParams(Paramstruct* params) {
    int binding = -3;
    while(params != NULL) {
        if(LAppend(params->name, params->type, binding)) {
            return 1;
        }
        binding--;
        params = params->next;
    }
    return 0;
}

/* Local variable i (1-based) is at BP+i */
int LInstall(char* name, int type) {
    if(LAppend(name, type, local_count + 1)) {
        return 1;
    }
    local_count++;
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

/* ---------------- Type table ---------------- */

static Typetable* ttable[MAX_TYPES];
static int tcount = 0;

int TCount() {
    return tcount;
}

int TLookup(char* name) {
    int i;
    for(i = 0; i < tcount; i++) {
        if(strcmp(ttable[i]->name, name) == 0) return ttable[i]->code;
    }
    return -1;
}

int TCreate(char* name) {
    if(TLookup(name) >= 0 || tcount >= MAX_TYPES) return -1;
    Typetable* t = (Typetable*) malloc(sizeof(Typetable));
    t->name = strdup(name);
    t->code = USER_TYPE_BASE + tcount;
    t->nfields = 0;
    t->fields = NULL;
    ttable[tcount++] = t;
    return t->code;
}

int TAddField(int code, char* name, int type) {
    Typetable* t = ttable[code - USER_TYPE_BASE];
    Field* f;
    Field* last = NULL;
    for(f = t->fields; f != NULL; f = f->next) {
        if(strcmp(f->name, name) == 0) return 1;
        last = f;
    }
    if(t->nfields >= MAX_FIELDS) return 2;
    f = (Field*) malloc(sizeof(Field));
    f->name = strdup(name);
    f->type = type;
    f->index = t->nfields++;
    f->next = NULL;
    if(last == NULL) t->fields = f; else last->next = f;
    return 0;
}

Field* FLookup(int code, char* name) {
    Field* f;
    if(!isUserType(code)) return NULL;
    for(f = ttable[code - USER_TYPE_BASE]->fields; f != NULL; f = f->next) {
        if(strcmp(f->name, name) == 0) return f;
    }
    return NULL;
}

const char* typeName(int type) {
    switch(type) {
        case INTEGER_TYPE: return "int";
        case STRING_TYPE: return "str";
        case NULL_TYPE: return "null";
        case BOOLEAN_TYPE: return "boolean";
    }
    if(isUserType(type) && type - USER_TYPE_BASE < tcount) return ttable[type - USER_TYPE_BASE]->name;
    return "?";
}

void print_type_table() {
    int i;
    Field* f;
    printf("---- Type Table ----\n");
    for(i = 0; i < tcount; i++) {
        printf("%s (code %d, %d fields):", ttable[i]->name, ttable[i]->code, ttable[i]->nfields);
        for(f = ttable[i]->fields; f != NULL; f = f->next) {
            printf(" %s:%s@%d", f->name, typeName(f->type), f->index);
        }
        printf("\n");
    }
}
