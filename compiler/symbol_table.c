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
    newentry->next = NULL;

    appendGlobal(newentry);
    return 0;
}

int newFunctionLabel() {
    return flabel_count++;
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
    newentry->flabel = newFunctionLabel();
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

/* Parameter i (1-based) is at BP-(i+2): [BP-1] = return address, [BP-2] = return value.
   Methods get the implicit self at BP-3, so their parameters start at BP-4. */
int LInstallParams(Paramstruct* params, int first_binding) {
    int binding = first_binding;
    while(params != NULL) {
        if(LAppend(params->name, params->type, binding)) {
            return 1;
        }
        binding--;
        params = params->next;
    }
    return 0;
}

int LInstallSelf(int class_type) {
    return LAppend("self", class_type, -3);
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

static Classtable* ctable[MAX_CLASSES];
static int ccount = 0;

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
    if(isClassType(type) && type - CLASS_TYPE_BASE < ccount) return ctable[type - CLASS_TYPE_BASE]->name;
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

/* ---------------- Class table ---------------- */

Classtable* Class_Entry(int code) {
    if(!isClassType(code) || code - CLASS_TYPE_BASE >= ccount) return NULL;
    return ctable[code - CLASS_TYPE_BASE];
}

int CLookup(char* name) {
    int i;
    for(i = 0; i < ccount; i++) {
        if(strcmp(ctable[i]->name, name) == 0) return ctable[i]->code;
    }
    return -1;
}

int CInstall(char* name) {
    if(TLookup(name) >= 0 || CLookup(name) >= 0 || ccount >= MAX_CLASSES) return -1;
    Classtable* c = (Classtable*) malloc(sizeof(Classtable));
    c->name = strdup(name);
    c->code = CLASS_TYPE_BASE + ccount;
    c->nfields = 0;
    c->fields = NULL;
    c->nmethods = 0;
    c->methods = NULL;
    c->parent = NULL;
    ctable[ccount++] = c;
    return c->code;
}

Field* Class_Flookup(int code, char* name) {
    Classtable* c = Class_Entry(code);
    Field* f;
    if(c == NULL) return NULL;
    for(f = c->fields; f != NULL; f = f->next) {
        if(strcmp(f->name, name) == 0) return f;
    }
    return NULL;
}

Method* Class_Mlookup(int code, char* name) {
    Classtable* c = Class_Entry(code);
    Method* m;
    if(c == NULL) return NULL;
    for(m = c->methods; m != NULL; m = m->next) {
        if(strcmp(m->name, name) == 0) return m;
    }
    return NULL;
}

int Class_Finstall(int code, char* name, int type) {
    Classtable* c = Class_Entry(code);
    Field* f;
    Field* last = NULL;
    for(f = c->fields; f != NULL; f = f->next) {
        if(strcmp(f->name, name) == 0) return 1;
        last = f;
    }
    if(c->nfields >= MAX_FIELDS) return 2;
    f = (Field*) malloc(sizeof(Field));
    f->name = strdup(name);
    f->type = type;
    f->index = c->nfields++;
    f->next = NULL;
    if(last == NULL) c->fields = f; else last->next = f;
    return 0;
}

/* Only one method per name is allowed (no overloading); a method name may not clash with a field */
Method* Class_Minstall(int code, char* name, int type, Paramstruct* params) {
    Classtable* c = Class_Entry(code);
    Method* m;
    Method* last = NULL;
    if(Class_Mlookup(code, name) != NULL || Class_Flookup(code, name) != NULL) return NULL;
    if(c->nmethods >= MAX_METHODS) return NULL;
    for(m = c->methods; m != NULL; m = m->next) last = m;
    m = (Method*) malloc(sizeof(Method));
    m->name = strdup(name);
    m->type = type;
    m->paramlist = params;
    m->flabel = newFunctionLabel();
    m->position = c->nmethods++;
    m->defined = 0;
    m->next = NULL;
    if(last == NULL) c->methods = m; else last->next = m;
    return m;
}

void print_class_table() {
    int i;
    Field* f;
    Method* m;
    printf("---- Class Table ----\n");
    for(i = 0; i < ccount; i++) {
        printf("%s (code %d, %d fields, %d methods, parent: NULL)\n", ctable[i]->name, ctable[i]->code, ctable[i]->nfields, ctable[i]->nmethods);
        for(f = ctable[i]->fields; f != NULL; f = f->next) {
            printf("  field  %s:%s@%d\n", f->name, typeName(f->type), f->index);
        }
        for(m = ctable[i]->methods; m != NULL; m = m->next) {
            printf("  method %s:%s position %d label F%d (", m->name, typeName(m->type), m->position, m->flabel);
            Paramstruct* p = m->paramlist;
            while(p != NULL) {
                printf("%s %s%s", typeName(p->type), p->name, p->next ? ", " : "");
                p = p->next;
            }
            printf(")\n");
        }
    }
}
