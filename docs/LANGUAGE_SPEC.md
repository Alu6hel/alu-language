# Alu Language Specification (v1.0 Subset)

This document outlines the exact subset of the Alu programming language that currently compiles successfully through the C++ frontend (`alu_cxx`) and is fully operational.

## 1. Data Types
The following primitive data types are fully supported:
- **`int`**: 32-bit signed integer.
- **`float`**: 32-bit floating-point number.
- **`double`**: 64-bit floating-point number.
- **`bool`**: Boolean value (`true` or `false`).
- **`byte`**: 8-bit unsigned integer.
- **`string`**: String literal type.
- **`void`**: Represents no value (used as return type).

### Pointers and Arrays
- **Pointer types**: Declared using the `*` suffix (e.g., `int*`, `struct MyStruct*`).
- **Array types**: Declared using brackets (e.g., `int myArray[10]`).

---

## 2. Declarations & Scoping

### Namespaces
Namespaces can be defined to group related routines, structs, and effects.
```alu
namespace Math {
    routine add(int a, int b) -> int {
        return a + b;
    }
}
```
Elements within a namespace are accessed using the double colon `::` operator (e.g., `Math::add(1, 2)`).

### Imports
Modules and files can be imported into the current scope.
```alu
import "other_file.alu";
import std::fs;
```

---

## 3. Routines (Functions)

### Routine Declarations
Functions are declared using the `routine` keyword.
```alu
routine calculate(int x, int y) -> int {
    return x + y;
}
```

### Method Receiver Syntax
Routines can be attached to types as methods using receiver syntax:
```alu
routine (file: File*) read() -> string {
    // implementation
}
```

### Extern Routines
External functions (e.g., C standard library or linked backend C++ code) are declared using `extern routine`. Variadic arguments are supported via `...`.
```alu
extern routine printf(string format, ...) -> int;
```

---

## 4. Structs

Structs define custom composite data types. Generic type parameters are supported syntactically.
```alu
struct Vector2<T> {
    float x;
    float y;
}
```

### Preconditions and Postconditions
Structs and routines can be annotated with contract annotations (verified statically by the Z3 integration).
```alu
[@requires(x > 0)]
[@ensures(return > 0)]
routine safe_math(int x) -> int {
    return x + 1;
}
```

---

## 5. Control Flow

The compiler supports the following control flow statements:
- **`if` / `else`**
- **`while`** loops
- **`for`** loops (`for (init; cond; update) { ... }`)
- **`return`**

---

## 6. Variables and Expressions

- **Declaration**: `int x = 10;`
- **Assignment**: `x = 20;`
- **Casting**: Explicit cast via `as` keyword: `10.5 as int`
- **Binary Operations**: Standard math/logic operators (`+`, `-`, `*`, `/`, `==`, `!=`, `>`, `<`, `>=`, `<=`, `&&`, `||`).
- **Function/Method Calls**: `print("Hello");` or `obj.method();`

---

## 7. Memory Management

Manual memory management is available for dynamic allocations:
- **Allocation**: `new Type`
- **Deallocation**: `free(ptr)`
- **Address-of**: `&var`
- **Dereference**: `*ptr`

```alu
int* ptr = new int;
*ptr = 42;
free(ptr);
```

---

## 8. Error Handling

Exceptions can be thrown and caught using `throw` and `try/catch`.
```alu
try {
    throw new Error;
} catch (Error e) {
    print("Caught an error");
}
```

---

## 9. Algebraic Effects

The Alu compiler implements algebraic effects for delimited continuations:
- **`effect`**: Defines a group of effectful operations.
- **`yield`**: Pauses execution and hands control to the handler.
- **`handle ... in`**: Provides a handler implementation for a specific effect.
- **`resume`**: Resumes the continuation from the handler.

```alu
effect Logger {
    routine log(string msg);
}

routine do_work() -> void {
    yield Logger.log("Starting work");
}

handle Logger {
    on log(string msg) {
        print(msg);
        resume;
    }
} in do_work();
```
