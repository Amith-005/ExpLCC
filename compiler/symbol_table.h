#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Type Constants */
#define INTEGER_TYPE 101
#define BOOLEAN_TYPE 102
#define STRING_TYPE 103
#define INTEGER_POINTER_TYPE 104
#define INTEGER_ARRAY_TYPE 105
#define STRING_ARRAY_TYPE 106
#define STRING_POINTER_TYPE 107

/* Tuple types get codes TUPLE_BASE + id, pointers to them TUPLE_PTR_BASE + id.
   Two tuple values have the same type only if they have the same code (name equivalence). */
#define TUPLE_BASE 1000
#define TUPLE_PTR_BASE 2000
#define isTupleType(t) ((t) >= TUPLE_BASE && (t) < TUPLE_PTR_BASE)
#define isTuplePtrType(t) ((t) >= TUPLE_PTR_BASE && (t) < TUPLE_PTR_BASE + 1000)

/* Formal parameter list of a function (also used for the field list of a tuple definition) */
typedef struct Paramstruct {
    char* name;
    int type;
    struct Paramstruct* next;
} Paramstruct;

/* Global symbol table: global variables and functions */
typedef struct Gsymbol {
    char* name;       // name of the variable / function
    int type;         // type of the variable / return type of the function
    int size;         // size of the variable (0 for functions)
    int* metadata;    // array dimensions: NULL for scalar, [size] for 1D, [rows, cols] for 2D
    int binding;      // static memory address allocated to the variable (-1 for functions)
    Paramstruct* paramlist; // formal parameters (functions only)
    int flabel;       // function label number: F<flabel> (-1 for variables)
    int isFunction;   // 1 if this entry is a function
    int defined;      // 1 once the function definition has been seen
    struct Gsymbol *next;
} Gsymbol;

/* Local symbol table: formal parameters and local variables of one function */
typedef struct Lsymbol {
    char* name;
    int type;
    int binding;      // offset relative to BP (params: -3,-4,..  locals: 1,2,..)
    struct Lsymbol* next;
} Lsymbol;

/* Tuple type table */
typedef struct Fieldlist {
    char* name;
    int type;
    int offset;       // offset of the field from the start of the tuple
    struct Fieldlist* next;
} Fieldlist;

typedef struct Typetable {
    char* name;
    int typecode;     // TUPLE_BASE + id
    int size;         // number of words
    Fieldlist* fields;
    struct Typetable* next;
} Typetable;

Typetable* TLookup(char* name);
Typetable* TLookupByCode(int typecode);
int TInstall(char* name, Paramstruct* fields);   // returns the new type code, -1 if redefined, -2 on duplicate field
Fieldlist* FLookup(Typetable* t, char* fname);
int typeSize(int type);
int maxTupleSize();
void print_type_table();

/* Global symbol table */
Gsymbol* Lookup(char* name);
int Install(char* name, int type, int size, int* metadata);
int InstallFunction(char* name, int type, Paramstruct* paramlist);
void print_symbol_table();
int get_stack_pointer();
Gsymbol* get_global_head();

/* Parameter lists */
Paramstruct* makeParam(char* name, int type);
Paramstruct* appendParam(Paramstruct* list, Paramstruct* p);
int countParams(Paramstruct* list);

/* Local symbol table (one active table at a time: the function being parsed) */
void LReset();
Lsymbol* LLookup(char* name);
int LInstallParams(Paramstruct* params, int retsize);
int LInstall(char* name, int type, int size);
int LLocalCount();
void print_local_table(char* fname);

#endif
