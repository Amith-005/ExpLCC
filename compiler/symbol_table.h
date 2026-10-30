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
#define NULL_TYPE 108            /* type of the constant NULL */
#define ALLOC_TYPE 109           /* type of an alloc() call: assignable to any user-defined type */
#define USER_TYPE_BASE 200       /* user-defined type i has type code USER_TYPE_BASE + i */
#define CLASS_TYPE_BASE 300      /* class i has type code CLASS_TYPE_BASE + i */
#define isUserType(t) ((t) >= USER_TYPE_BASE && (t) < CLASS_TYPE_BASE)
#define isClassType(t) ((t) >= CLASS_TYPE_BASE)
#define MAX_TYPES 32
#define MAX_CLASSES 32
#define MAX_FIELDS 8             /* fixed heap block size is 8 words */
#define MAX_METHODS 8

/* Member field of a user-defined type */
typedef struct Field {
    char* name;
    int type;         // type code of the field
    int index;        // word offset inside the heap block
    struct Field* next;
} Field;

/* Type table: one entry per user-defined type (int/str/NULL are the implicit entries) */
typedef struct Typetable {
    char* name;
    int code;
    int nfields;
    Field* fields;
} Typetable;

/* Formal parameter list of a function */
typedef struct Paramstruct {
    char* name;
    int type;
    struct Paramstruct* next;
} Paramstruct;

/* Method of a class */
typedef struct Method {
    char* name;
    int type;                 // return type
    Paramstruct* paramlist;   // formal parameters (without the implicit self)
    int flabel;               // function label number: F<flabel>
    int position;             // index in the method list = index in the virtual function table
    int owner;                // class that declared (or last overrode) the method
    int defined;              // 1 once the method definition has been seen
    struct Method* next;
} Method;

/* Class table: one entry per class (no inheritance in this stage) */
typedef struct Classtable {
    char* name;
    int code;
    int nfields;                 // words used by member fields (class-typed fields take 2 words)
    Field* fields;
    int nmethods;
    Method* methods;
    struct Classtable* parent;   // NULL if the class extends nothing
} Classtable;

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
int LInstallParams(Paramstruct* params, int first_binding);
int LInstallSelf(int class_type);
int LInstall(char* name, int type);
int LLocalCount();
void print_local_table(char* fname);

/* Type table */
int TCreate(char* name);                 // returns type code, -1 if the name is already a type
int TLookup(char* name);                 // type code, -1 if undefined
int TAddField(int code, char* name, int type); // 0 ok, 1 duplicate field, 2 too many fields
Field* FLookup(int code, char* name);
int TCount();
const char* typeName(int type);
void print_type_table();

/* Class table */
int CInstall(char* name, int parent);              // class type code, -1 if the name is already a type/class; parent: class type code or -1
int CCount();
int isSubclass(int sub, int sup);                  // 1 if sub is sup or a descendent of sup
void reserveVFT();                                 // reserve 8 words per class below the global variables
int CLookup(char* name);                           // class type code, -1 if undefined
int Class_Finstall(int code, char* name, int type);  // 0 ok, 1 duplicate field, 2 too many fields
int Class_Minstall(int code, char* name, int type, Paramstruct* params); // 0 ok, 1 duplicate/clash with a field, 2 too many methods, 3 override with a different signature
Field* Class_Flookup(int code, char* name);
Method* Class_Mlookup(int code, char* name);
Method* Class_MlookupSig(int code, char* name, Paramstruct* params);   // exact parameter types
Classtable* Class_Entry(int code);
int newFunctionLabel();
void print_class_table();

#endif
