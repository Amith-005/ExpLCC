# ExpLCC

**ExpLCC** is a stage-wise compiler for the **Experimental Programming Language (ExpL)**, implemented in **C using Flex and Bison/YACC**, and targeting the **Experimental String Machine (XSM)**.

The compiler is developed incrementally, with each stage extending the previous implementation. The Git history preserves this evolution from a basic arithmetic-expression compiler to a compiler supporting variables, control flow, symbol tables, arrays, pointers, functions, recursion, local scope, user-defined types, and dynamic memory management.

The current implementation covers **Stage 1 through Stage 6**.

---

## Highlights

- Lexical analysis using Flex
- LR parsing using Bison/YACC
- Abstract Syntax Tree (AST) construction
- Semantic analysis and type checking
- Global and Local Symbol Tables
- Control-flow code generation
- Label Address Table (LAT) and label translation
- Arrays and pointers
- Functions and recursion
- Runtime stack-frame management
- User-defined types
- Dynamic heap allocation
- XSM target-code generation

---

# Compiler Evolution

## Stage 1 — Arithmetic Expression Compiler

Stage 1 introduced the basic compiler pipeline.

### Implemented

- Lexical analysis using Flex
- Parsing using YACC/Bison
- Integer constants
- Arithmetic operators: `+`, `-`, `*`, `/`
- Operator precedence
- Parenthesized expressions
- Abstract Syntax Tree construction
- Register allocation using `getReg()` and `freeReg()`
- Recursive AST-based code generation
- XSM `Write` library call
- XSM program termination

Stage 1 established the basic AST-based expression compiler.

---

## Stage 2 — Variables, Assignment and I/O

Stage 2 extended the compiler from individual arithmetic expressions to complete straight-line programs.

### Newly Implemented

- Variables
- Memory allocation beginning from address `4096`
- Assignment statements
- Multiple statement execution
- `read()`
- `write()`
- Variable load and store operations
- XSM `Read` library call
- XSM `Write` library call
- Additional AST nodes for variables, constants, assignments, statements, read, and write

Example:

```c
begin
    read(a);
    b = a + 10;
    write(b);
end;
```

---

## Stage 3 — Control Flow and Label Translation

Stage 3 introduced conditional and iterative control flow.

### Newly Implemented

- Relational operators: `<`, `>`, `<=`, `>=`, `==`, `!=`
- Boolean expressions
- Conditional type checking
- `if`
- `if-else`
- `while`
- `repeat-until`
- `do-while`
- `break`
- `continue`
- Symbolic label generation
- `getLabel()`
- XSM jump instructions: `JMP`, `JZ`, `JNZ`

Example generated structure:

```text
L0:
    condition
    JZ R0, L1

    body

    JMP L0
L1:
```

### Label Address Table

Stage 3 introduced the **Label Address Table (LAT)**.

The code generator initially produces symbolic labels such as:

```text
L0
L1
L2
```

The label translator performs two passes:

1. Scan the generated XSM code and construct the LAT.
2. Replace symbolic labels in jump instructions with actual XSM instruction addresses.

Example:

```text
L0 -> 2056
L1 -> 2082
```

Stage 3 transformed the compiler from straight-line execution into a compiler capable of implementing control flow.

---

## Stage 4 — Global Symbol Table, Arrays and Pointers

Stage 4 introduced declarations and symbol-table-based memory management.

### Newly Implemented

- `decl ... enddecl`
- Integer declarations
- String declarations
- Global Symbol Table (**GST**)
- `GSTLookup()`
- `GSTInstall()`
- Symbol metadata: name, type, size, binding
- Symbol-table-based memory bindings
- Undeclared-variable detection
- Type checking
- One-dimensional arrays
- Two-dimensional arrays
- Array address calculation
- Pointer declarations
- Address-of operator `&`
- Dereference operator `*`
- Pointer-to-integer types
- Pointer-to-string types
- Pointer assignment validation
- Improved `break` and `continue` handling for nested loops

---

## Stage 5 — Functions, Local Scope, Recursion and Tuples

Stage 5 introduced functions and function-level runtime organization.

### Newly Implemented

- Function declarations
- Function definitions
- Function calls
- Return statements
- Function return values
- Formal parameters
- Actual parameters
- Argument count checking
- Argument type checking
- Function labels
- Local Symbol Table (**LST**)
- Local variables
- Function parameters
- Local-first / global-second symbol lookup
- `BP`-relative addressing
- Activation records
- Function stack frames
- Register save and restore during function calls
- Recursive functions
- Function return-type checking
- Function declaration/definition consistency checks
- Tuple type support
- Tuple fields
- Tuple variables
- Tuple pointers
- Field access using `.`
- Pointer field access using `->`
- Tuple assignment

Stage 5 enabled recursive programs and introduced proper runtime support for function execution.

---

## Stage 6 — User-Defined Types and Dynamic Memory Management

Stage 6 extends the compiler with **user-defined data types and dynamic heap memory management**.

This allows ExpL programs to create dynamically allocated recursive structures such as linked lists and trees.

### Newly Implemented

- `type ... endtype` blocks
- User-defined type declarations
- Type Table management
- Field lists
- Type creation and lookup
- Field lookup and validation
- Member access using `.`
- Nested member access
- Arrays of user-defined types
- `null`
- `initialize()`
- `alloc()`
- `free()`
- AST support for initialization, allocation and deallocation
- Dynamic heap management
- Fixed-size heap blocks
- Free-list-based memory allocation
- Heap initialization routines
- Heap allocation routines
- Heap deallocation routines
- Type compatibility checks between user-defined types and `null`

Example user-defined type:

```c
type
    node {
        int val;
        node next;
    }
endtype
```

A variable of this type can be declared using:

```c
decl
    node head;
enddecl
```

Memory can then be dynamically allocated:

```c
head = alloc();
head.val = 10;
head.next = null;
```

Memory can later be released using:

```c
free(head);
```

Stage 6 enables the compiler to support dynamically allocated recursive data structures.

---

# Compilation Pipeline

```text
ExpL Source Program
        │
        ▼
Lexical Analysis
        │
        ▼
Syntax Analysis
        │
        ▼
Semantic Analysis
        │
        ▼
Abstract Syntax Tree
        │
        ▼
Symbol / Type Table Resolution
        │
        ▼
XSM Code Generation
        │
        ▼
Assembly with Symbolic Labels
        │
        ▼
Label Translation
        │
        ▼
Final XSM Assembly
        │
        ▼
XSM Simulator
```

---

# Repository Structure

```text
ExpLCC/
│
├── compiler/
│   ├── compiler.l
│   ├── compiler.y
│   ├── compiler.c
│   ├── compiler.h
│   ├── symbol_table.c
│   ├── symbol_table.h
│   ├── translate.l
│   └── translate.c
│
├── xsm_dev/
│   └── XSM simulator source
│
├── xfs-interface/
│   └── XSM filesystem interface
│
├── .gitignore
└── README.md
```

The `compiler/` directory contains the latest compiler implementation.

The Git commit history preserves the compiler's evolution through each stage.

---

# Core Compiler Components

## Lexical Analyzer

Implemented using **Flex**.

It recognizes identifiers, constants, keywords, arithmetic operators, relational operators, type keywords, control-flow keywords, delimiters, and memory-management keywords.

## Parser

Implemented using **YACC/Bison**.

The parser validates program syntax, applies grammar rules, handles operator precedence, constructs the AST, performs semantic checks, and verifies type correctness.

## Abstract Syntax Tree

The AST acts as the central intermediate representation of the compiler.

AST nodes represent constants, variables, operators, assignments, read/write statements, conditionals, loops, arrays, pointers, functions, function calls, return statements, user-defined types, field access, and dynamic memory operations.

The code generator recursively traverses the AST and generates XSM instructions.

---

# Symbol and Type Management

## Global Symbol Table

The GST stores globally visible entities such as variables, arrays, pointers, functions, type information, memory bindings, and function metadata.

## Local Symbol Table

The LST stores entities local to functions, including parameters and local variables.

Local variables and parameters are accessed relative to the **Base Pointer (`BP`)**.

## Type Table

Stage 6 introduces a Type Table for managing built-in and user-defined types.

It maintains information about:

- type names
- type sizes
- fields
- field types
- recursive type relationships

Field lookup allows nested field expressions to be type checked and translated correctly.

---

# Runtime Organization

The compiler manages:

- global memory
- temporary registers
- runtime stack
- function parameters
- return values
- local variables
- activation records
- saved registers
- dynamically allocated heap blocks

Functions use stack frames, while Stage 6 adds heap-based memory allocation for dynamically created objects.

---

# Dynamic Memory Management

Stage 6 introduces runtime heap management.

The main ExpL operations are:

```c
initialize();
alloc();
free();
```

### `initialize()`

Initializes the heap before dynamic allocation is performed.

### `alloc()`

Allocates a block of memory from the heap and returns its address.

### `free()`

Returns a previously allocated block to the free list.

---

# Register Allocation

Temporary registers are managed using:

```c
getReg();
freeReg();
```

---

# Target Machine

ExpLCC generates assembly code for the **Experimental String Machine (XSM)**.

Generated instructions include:

```text
MOV
ADD
SUB
MUL
DIV
LT
GT
LE
GE
EQ
NE
JMP
JZ
JNZ
PUSH
POP
CALL
RET
```

---

# Technology Stack

- **Implementation Language:** C
- **Lexer Generator:** Flex / Lex
- **Parser Generator:** Bison / YACC
- **Target Architecture:** Experimental String Machine (XSM)
- **Intermediate Representation:** Abstract Syntax Tree
- **Symbol Management:** GST + LST
- **Type Management:** Type Table + Field Lists
- **Runtime Model:** Stack Frames + Dynamic Heap
- **Code Generation:** XSM Assembly

---

# Concepts Demonstrated

ExpLCC demonstrates:

- lexical analysis
- LR parsing
- syntax-directed translation
- abstract syntax trees
- semantic analysis
- static type checking
- symbol tables
- type tables
- scope resolution
- memory binding
- arrays
- pointers
- control-flow translation
- symbolic labels
- Label Address Table
- register allocation
- runtime stack organization
- activation records
- functions
- recursion* target code generation

---

## Development History

The repository uses Git commits to preserve each major compiler milestone.

```text
Stage 1
Arithmetic Expression Compiler
        ↓
Stage 2
Variables + Assignment + I/O
        ↓
Stage 3
Control Flow + Labels + LAT
        ↓
Stage 4
GST + Arrays + Pointers
        ↓
Stage 5
Functions + LST + Recursion + Tuples
```

Each stage builds on the previous implementation rather than existing as an independent compi>



- user-defined types
- recursive data structures
- dynamic memory allocation
- free-list memory management
- target code generation

---

# Development History

```text
Stage 1
Arithmetic Expression Compiler
        ↓
Stage 2
Variables + Assignment + I/O
        ↓
Stage 3
Control Flow + Labels + LAT
        ↓
Stage 4
GST + Arrays + Pointers
        ↓
Stage 5
Functions + LST + Recursion + Tuples
        ↓
Stage 6
User-Defined Types + Dynamic Memory Management
```

Each stage builds upon the previous implementation rather than existing as an independent compiler copy.

The Git commit history therefore provides a direct record of the compiler's development from Stage 1 through Stage 6.
