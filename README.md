# ExpLCC

**ExpLCC** is a compiler for the **Experimental Programming Language (ExpL)**, implemented in **C using Flex and Bison/YACC**, and targeting the **Experimental String Machine (XSM)**.

The compiler was developed incrementally across multiple stages, with each stage extending the previous implementation. The Git history preserves this evolution from a basic arithmetic-expression compiler to a compiler supporting control flow, symbol tables, arrays, pointers, functions, recursion, local scope, stack frames, and user-defined tuple types.

---

## Compiler Evolution

### Stage 1 — Arithmetic Expression Compiler

The first stage introduced the basic compiler pipeline.

Implemented:

* Lexical analysis using Flex
* Parsing using YACC/Bison
* Integer constants
* Arithmetic operators:

  * `+`
  * `-`
  * `*`
  * `/`
* Operator precedence
* Parenthesized expressions
* Abstract Syntax Tree construction
* Register allocation using `getReg()` and `freeReg()`
* Recursive AST-based XSM code generation
* XSM output using `Write`
* Program termination using the XSM `Exit` library call

This stage established the basic AST-based expression compiler.

---

### Stage 2 — Variables, Assignment and I/O

Stage 2 extended the compiler from evaluating individual expressions to compiling complete straight-line programs.

Newly implemented:

* Variable support
* Memory allocation beginning from address `4096`
* Assignment statements
* Multiple statements
* `read()` statements
* `write()` statements
* Variable load and store operations
* XSM `Read` library call
* XSM `Write` library call
* Additional AST node types for:

  * variables
  * constants
  * statements
  * assignment
  * read
  * write

Example:

```c
begin
    read(a);
    b = a + 10;
    write(b);
end;
```

---

### Stage 3 — Control Flow and Label Translation

Stage 3 introduced non-linear control flow.

Newly implemented:

* Relational operators:

  * `<`
  * `>`
  * `<=`
  * `>=`
  * `==`
  * `!=`
* Boolean expressions
* Conditional type checking
* `if`
* `if-else`
* `while`
* `repeat-until`
* `do-while`
* `break`
* `continue`
* Symbolic label generation using `getLabel()`
* XSM jump instructions:

  * `JZ`
  * `JNZ`
  * `JMP`

Stage 3 also introduced **label translation**.

The compiler initially generates symbolic labels such as:

```text
L0:
...
JZ R0, L1
...
JMP L0
L1:
```

A separate label translator then converts these symbolic labels into actual XSM instruction addresses.

### Label Address Table — LAT

The label translator uses a **Label Address Table (LAT)**.

Example:

```text
L0 -> 2056
L1 -> 2082
```

Label translation is performed using two passes:

1. Scan the generated assembly and build the LAT.
2. Replace symbolic labels in jump instructions with actual XSM addresses.

This stage transformed the compiler from a straight-line compiler into one capable of conditional and iterative execution.

---

### Stage 4 — Global Symbol Table, Arrays and Pointers

Stage 4 introduced declaration handling and symbol-table-based memory management.

Newly implemented:

* Declaration blocks
* `int` variables
* `str` variables
* Global Symbol Table — **GST**
* `GSTLookup()`
* `GSTInstall()`
* Symbol metadata:

  * name
  * type
  * size
  * binding
* Symbol-table-based memory bindings
* Undeclared-variable detection
* Type checking
* One-dimensional arrays
* Two-dimensional arrays
* Array address calculation
* Pointer declarations
* Address-of operator `&`
* Dereference operator `*`
* Pointer-to-integer types
* Pointer-to-string types
* Pointer assignment validation
* Improved loop handling for `break` and `continue`

Instead of calculating variable addresses directly from variable names, Stage 4 uses the symbol-table binding stored with each symbol.

Example concept:

```c
t->GSTentry->binding
```

---

### Stage 5 — Functions, Local Scope, Recursion and Tuples

Stage 5 extends ExpLCC with function-level scope and runtime stack-frame management.

Newly implemented:

* Function declarations
* Function definitions
* Function calls
* Return statements
* Function return values
* Formal parameters
* Actual parameters
* Parameter count checking
* Parameter type checking
* Function labels
* Local Symbol Table — **LST**
* Local variables
* Function parameters
* Local-first / global-second symbol lookup
* `BP`-relative addressing
* Function stack frames
* Activation record management
* Saving registers before function calls
* Restoring registers after function calls
* Recursive functions
* Return-type validation
* Function declaration/definition consistency checking

Example supported concept:

```c
int factorial(int n)
{
    if (n == 0)
        return 1;

    return n * factorial(n - 1);
}
```

Stage 5 also introduces user-defined tuple types.

Tuple functionality includes:

* Tuple type definitions
* Tuple fields
* Tuple variables
* Tuple pointers
* Field access using `.`
* Pointer field access using `->`
* Tuple assignment
* Tuple parameters
* Tuple return values
* Tuple-related type checking

---

## Compilation Pipeline

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
Symbol Table Resolution
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

## Repository Structure

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

The `compiler/` directory contains the current compiler implementation.

The Git commit history preserves the compiler's stage-by-stage evolution.

---

## Core Compiler Components

### Lexical Analyzer

Implemented using **Flex**.

It identifies tokens such as:

* identifiers
* constants
* keywords
* arithmetic operators
* relational operators
* delimiters
* control-flow keywords
* type keywords

---

### Parser

Implemented using **YACC/Bison**.

The parser:

* validates syntax
* enforces grammar rules
* applies operator precedence
* constructs the AST
* triggers semantic checks

---

### Abstract Syntax Tree

The AST acts as the central intermediate representation of the compiler.

AST nodes represent constructs such as:

* constants
* variables
* operators
* assignment
* read/write
* conditionals
* loops
* functions
* function calls
* return statements
* arrays
* pointers
* tuples

The code generator recursively traverses the AST to produce XSM instructions.

---

## Symbol Tables

### Global Symbol Table

The GST stores globally visible entities such as:

* variables
* arrays
* pointers
* functions
* type information
* memory bindings
* function metadata

---

### Local Symbol Table

The LST stores identifiers local to a function, including:

* parameters
* local variables

Local variables and parameters are accessed relative to the **Base Pointer (`BP`)**.

---

## Runtime Organization

The compiler manages:

* global memory
* registers
* runtime stack
* function parameters
* return values
* local variables
* activation records
* saved registers

Function calls use stack frames to preserve execution state and support recursion.

---

## Register Allocation

Expression evaluation uses temporary XSM registers.

The compiler manages registers using:

```c
getReg();
freeReg();
```

For example:

```text
MOV R0, 5
MOV R1, 10
ADD R0, R1
```

After the addition, `R1` can be released while `R0` contains the result.

---

## Target Machine

ExpLCC generates code for the **Experimental String Machine (XSM)**.

Examples of generated instructions include:

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

## Technology Stack

* **Implementation Language:** C
* **Lexer Generator:** Flex / Lex
* **Parser Generator:** Bison / YACC
* **Target Machine:** XSM
* **Intermediate Representation:** Abstract Syntax Tree
* **Symbol Management:** GST + LST
* **Runtime Model:** Stack-based activation records

---

## Concepts Demonstrated

This project demonstrates several fundamental compiler-construction concepts:

* lexical analysis
* LR parsing
* syntax-directed translation
* abstract syntax trees
* semantic analysis
* static type checking
* symbol tables
* lexical and local scope
* memory binding
* arrays
* pointers
* control-flow translation
* label generation
* label backpatching/translation
* register allocation
* runtime stack organization
* activation records
* function calls
* recursion
* user-defined tuple types
* target code generation

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

Each stage builds on the previous implementation rather than existing as an independent compiler copy.
