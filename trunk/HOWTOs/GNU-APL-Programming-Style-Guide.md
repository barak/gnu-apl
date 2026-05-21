# GNU APL 2.0 Programming Style Guide

Derived from the actual patterns in `src/*.cc` and `src/*.hh`.

---

## 1. Indentation and Whitespace

**3 spaces** per indentation level, consistently throughout the entire codebase.
No tabs.

```cpp
void
Command::process_lines()
{
UCS_string line;
bool eof = false;

   for (;;)
      {
        if (!get_line(...))   break;
      }
}
```

Note that the *first* level of statements inside a function body often has **no** indentation
(the opening brace is in column 1), and subsequent nesting adds 3 spaces.  In practice the
common pattern is: declarations at column 0, the first statement-block indented 3 spaces,
each nested block another 3.

One blank line separates logically distinct sections inside a function.
No trailing whitespace.

---

## 2. Line Length

Soft limit of **80 characters**.  Long function signatures are split across lines; continuation
parameters are aligned under the first parameter:

```cpp
Cell * get_member(const vector<const UCS_string *> & members,
                  Value * & owner, bool throw_error);
```

---

## 3. Brace Placement

**Opening braces** always appear on the *same line* as the controlling keyword or class/struct
head.  **Closing braces** are on their own line.

```cpp
// function definition — return type on its own line, signature + '{' together
Token
ScalarFunction::eval_scalar_B(Value_P B, prim_f1 fun) const
{
   ...
}

// class declaration
class Value : public DynamicObject
{
public:
   ...
};

// control flow
if (len_Z == 0)
   {
     ...
   }
```

Single-statement bodies may omit braces and be written on the same line as the condition,
separated by **three spaces**:

```cpp
if (len_Z == 0)   return do_eval_fill_B(B);
```

---

## 4. Separator Lines

Every function definition in a `.cc` file is preceded by a separator line of exactly
**76 hyphens** (total line length 78 characters):

```cpp
//----------------------------------------------------------------------------
Token
ScalarFunction::eval_scalar_B(Value_P B, prim_f1 fun) const
{
```

The same separator is used between major sections inside class declarations in headers.

---

## 5. Naming Conventions

### Classes and structs
PascalCase: `Value`, `Symbol`, `Token`, `Shape`, `Workspace`.

APL built-in function classes follow a fixed scheme:

| Prefix       | Meaning                          | Example              |
|--------------|----------------------------------|----------------------|
| `Bif_F1_`    | Monadic primitive function       | `Bif_F1_EXECUTE`     |
| `Bif_F2_`    | Dyadic primitive function        | `Bif_F2_AND`         |
| `Bif_F12_`   | Ambivalent primitive function    | `Bif_F12_PLUS`       |
| `Bif_OPER1_` | Monadic operator                 | `Bif_OPER1_EACH`     |
| `Bif_OPER2_` | Dyadic operator                  | `Bif_OPER2_POWER`    |
| `Quad_`      | System function / variable       | `Quad_FX`, `Quad_CR` |

### Member functions
`snake_case`.  Standard prefixes:

| Prefix  | Use                     | Example              |
|---------|-------------------------|----------------------|
| `get_`  | Accessor (read)         | `get_shape()`        |
| `set_`  | Mutator (write)         | `set_shape_item()`   |
| `is_`   | Boolean predicate       | `is_scalar()`        |
| `do_`   | Imperative action       | `do_eval_fill_B()`   |
| `PF_`   | Pool (parallel) function| `PF_scalar_B()`      |

### Member variables
No prefix or suffix convention.  Descriptive `snake_case`: `value_stack`, `owner_count`,
`monitor_callback`.

### Local variables
`snake_case`, often abbreviated when the scope is very small (`len_Z`, `ec`, `si`).

### Constants and macros
`ALL_CAPS_WITH_UNDERSCORES`: `DOMAIN_ERROR`, `LOC`, `MAX_RANK`.

Configuration values use a `cfg_` prefix: `cfg_SHORT_VALUE_LENGTH_WANTED`.

### Enum values
`ALL_CAPS`: `NC_VARIABLE`, `NC_FUNCTION`, `E_DOMAIN_ERROR`, `TOK_APL_VALUE1`.

### Type aliases
Always `typedef`, never `using`:

```cpp
typedef int64_t  ShapeItem;
typedef int16_t  sRank;
typedef int64_t  APL_Integer;
typedef const Function * cFunction_P;
```

Pointer-alias names carry a `_P` suffix (`cFunction_P`, `Value_P`).  Signed/unsigned pairs
are named with `s`/`u` prefix (`sRank`/`uRank`, `sAxis`/`uAxis`).

---

## 6. `const` Placement

Always `const T &` (const-before-type), never `T const &`:

```cpp
const Shape & get_shape() const;
void foo(const UCS_string & name, const Value_P & val);
const Value_P * locate_L(const UCS_string & function);
```

`const` on member functions goes at the end of the signature, after the closing parenthesis.

---

## 7. Header File Layout

### Include guards
Use the double-underscore `__NAME_HH_DEFINED__` macro pattern (no `#pragma once`):

```cpp
#ifndef __VALUE_HH_DEFINED__
#define __VALUE_HH_DEFINED__

...

#endif   // __VALUE_HH_DEFINED__
```

### Include order
1. System / library headers in `< >`, alphabetically.
2. Project headers in `" "`, alphabetically.
3. Optional `.icc` inline-implementation file last (`.cc` files only).

```cpp
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include "Common.hh"
#include "Symbol.hh"
#include "Workspace.hh"
```

Conditional includes are grouped with their guards:

```cpp
#if HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif
```

### Class section order
`public:` first, then `protected:`, then `private:` (rare).  Constructors and the destructor
appear at the top of the `public:` section.  Data members are at the bottom of whatever
section owns them.

---

## 8. Documentation Comments

All public API in headers is documented with Doxygen `///` (brief) or `/** … */` (detailed).
`\b word` marks up bold/parameter names.  Implementation notes in `.cc` files use plain `//`.

```cpp
/// create a system symbol with Id \b id
Symbol(Id id);

/// return the number of elements in this Value
ShapeItem element_count() const
   { return shape.get_volume(); }
```

Inline explanatory comments use `//` with two spaces before the `//`:

```cpp
Bif_F2_AND   Bif_F2_AND::fun;   // ∧
```

---

## 9. Inline Functions

Short (1–3 line) functions are defined inline in the header:

```cpp
bool is_scalar() const
   { return shape.get_rank() == 0; }
```

Longer inline bodies use the same brace style as out-of-line functions.  Do not use the
`inline` keyword explicitly; placing the body in the header is sufficient.

Larger implementation fragments that must live in the header for template reasons use a
separate `.icc` file that is `#include`d at the bottom of the `.cc` file.

---

## 10. Function Definitions in `.cc` Files

Return type on its own line, signature + opening brace on the next:

```cpp
//----------------------------------------------------------------------------
Token
ScalarFunction::eval_scalar_B(Value_P B, prim_f1 fun) const
{
const ShapeItem len_Z = B->element_count();
   ...
}
```

---

## 11. Loop Idioms

Use the project-wide `loop` macro (defined in `Common.hh`) for simple 0-based integer
iteration:

```cpp
// expands to: for (ShapeItem v = 0, __end__ = n; v < __end__; ++v)
loop(v, n)   process(ravel[v]);
```

Use standard `for` when the index is not 0-based, descends, or the condition is complex:

```cpp
for (sRank r = rho_rho - 1; r >= 0; --r)
   shape.add_shape_item(B->get_shape_item(r));
```

`for (;;)` for infinite loops that break on a condition.

---

## 12. Error Handling

Errors are raised via macros defined in `Error_macros.hh`.  These call `throw_apl_error()`
and are marked `[[noreturn]]`.  Never throw raw exceptions for APL-level errors.

```cpp
if (rank > MAX_RANK)   LIMIT_ERROR_RANK;
if (B->is_empty())     DOMAIN_ERROR;
if (!B)                VALUE_ERROR;
```

The `LOC` macro (expands to `__FILE__ ":" STR(__LINE__)`) is passed automatically by all
error macros and should be passed to any function that accepts a `const char * loc` parameter.

Use `Assert(condition)` for internal invariants that should never fail in correct code, and
`Assert1(condition)` for cheaper release-disabled checks.

Extend error messages before throwing with:

```cpp
MORE_ERROR() << "⌹[3]: argument must be numeric";
DOMAIN_ERROR;
```

---

## 13. Memory Management and Smart Pointers

`Value` objects are owned through `Value_P`, a custom reference-counting smart pointer.
Never hold a raw owning `Value *`.

```cpp
Value_P Z(B->get_shape(), LOC);   // allocate
Z->next_ravel_Cell() ...;
Z.check_value(LOC);
return Token(TOK_APL_VALUE1, Z);
```

Validity is tested with `operator+` (`+val` is true when non-null) and `operator!`:

```cpp
if (!result)   VALUE_ERROR;
Assert(+B);
```

All `Value_P` constructors and many allocation sites take a `loc` argument; always pass `LOC`.

Non-owning references to `Value` use `const Value *` or `Value *` raw pointers and are named
with a `_wptr` or `_cptr` suffix to signal non-ownership.

Other objects (e.g., `StateIndicator`, `Executable`) are managed with raw `new`/`delete`.

---

## 14. Parallel / Thread-Pool Functions

Functions that execute on the thread pool have the `PF_` prefix and must match the
`Thread_context::PoolFunction` typedef exactly:

```cpp
typedef void PoolFunction(Thread_context & ctx);
```

These are assigned to `Thread_context::do_work` before forking and must therefore keep the
non-`const` reference signature even if the body happens not to mutate `ctx`.

---

## 15. Integer Type Selection

| Type          | When to use                                      |
|---------------|--------------------------------------------------|
| `ShapeItem`   | Array dimensions, element indices, loop bounds   |
| `sRank`/`uRank` | Array rank values                              |
| `sAxis`/`uAxis` | Axis indices                                   |
| `APL_Integer` | APL-visible integer values                       |
| `APL_Float`   | APL-visible floating-point values                |
| `int`         | Small, local-scope counters not tied to arrays   |
| `size_t`      | Only when interfacing with C++ STL APIs          |

Avoid mixing `int` and `ShapeItem` in the same expression without a cast.

---

## 16. Standard Library Usage

`using namespace std;` is in effect globally (pulled in by `Common.hh`).

`std::vector` is used for dynamically-sized sequences.  `UCS_string` replaces `std::string`
for all APL text.  `Simple_string<T>` (a project template) replaces `std::vector<T>` in some
performance-sensitive containers.  There is no use of `std::unique_ptr` or `std::shared_ptr`;
`Value_P` fills that role for `Value` objects.

C++11 features (range-for, `auto`, move semantics, `nullptr`) are **not** used; the code
targets C++03/C++11 with a conservative subset.  Prefer explicit types over `auto`, and `0`
over `nullptr`.

---

## 17. Miscellaneous

- **`operator=`** returns `void`, not a reference (deviates from standard convention but is
  applied uniformly).
- **Friend classes** are used to give `Value_P` and internal job classes access to `Value`
  internals; document them with a comment.
- **Logging** uses the `Log(LOG_xxx)` macro; the body is a single expression after the macro
  with no braces: `Log(LOG_archive)   CERR << "saving..." << endl;`
- **Performance measurement** uses `PERFORMANCE_START` / `PERFORMANCE_END` macros; do not
  add these to new code unless the surrounding functions already use them.
- **Conditional compilation** for optional features (`HAVE_*`, `PARALLEL_ENABLED`, platform
  guards) uses `#if` / `#ifdef`, not `__attribute__` guards.
