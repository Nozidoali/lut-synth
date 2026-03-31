# Coding Style and File Naming Conventions

This document defines the coding style and file naming conventions for the lut-synth project.

## File Naming

- Use lowercase with hyphens for file names (e.g., `truth-table.cpp`, `xag-synthesizer.hpp`)
- No underscores in file names, use hyphens instead
- No abbreviations, use full words (e.g., `configuration.hpp` not `config.hpp`)
- Header files: `.hpp` for C++ headers, `.h` only when necessary for C compatibility
- Source files: `.cpp` for C++ implementation files

## Directory Structure

### External Libraries
- **Third-party projects** (e.g., mockturtle): Place under `third-party/` directory
  - Example: `third-party/mockturtle/`
  - These should be integrated via `add_subdirectory()` in CMakeLists.txt

## Coding Style

### General Principles
- **No comments in cpp files**: Skip comments in implementation files. Code should be self-explanatory through clear naming
- **Document hpp files**: Use Doxygen-style comments for public API in header files (see Header Documentation section)
- **Declarations in headers, implementations in cpp**: Headers contain only declarations and documentation. Move all non-template function bodies to .cpp files (see Header/Implementation Separation). Headers and .cpp files live side-by-side in `src/lut-synth/`
- **Short and clean**: Write concise, readable code. Avoid verbosity
- **No try-catch blocks**: Avoid exception handling with try-catch. Use assertions instead
- **Minimal error handling**: Minimize error handling logic. Prefer assertions for invariants
- **Minimal print messages**: Avoid debug prints and verbose logging. Keep output minimal
- **No special characters**: Never use special characters in code or strings. Use only alphanumeric characters and underscores in code. File names use hyphens instead of underscores

### Assertions
- Use assertions (`assert()`) when features can be derived or when invariants must hold
- Prefer assertions over runtime error checks for conditions that should always be true
- Example: `assert(n > 0);` instead of `if (n <= 0) { /* error handling */ }`

### Code Structure
- Keep functions short and focused
- Use meaningful variable and function names
- Prefer inline functions for small, frequently used operations
- Use `const` and `constexpr` where appropriate
- Prefer range-based for loops: `for (auto const& item : container)`
- **No inline implementations**: Except for templates, constexpr, and trivial one-liners, all function bodies belong in .cpp files

### Header/Implementation Separation

Strict separation between declarations and implementations for maintainability and fast code review.

**Headers (.hpp) contain:**
- Class/struct declarations with member variables
- Function declarations (signatures only)
- Doxygen documentation comments
- Template definitions (exception - must be in headers)
- `constexpr` functions (exception - evaluated at compile time)
- Trivial one-line getters/setters (return member directly)

**Implementation files (.cpp) contain:**
- All function bodies (non-template, non-constexpr)
- Constructor/destructor implementations
- Operator implementations
- Algorithm logic

**Examples:**

Good header (declaration only):
```cpp
class Synthesizer {
public:
    void synthesize(kitty::dynamic_truth_table const& tt);
    uint32_t and_count() const { return and_count_; }  // trivial getter OK
private:
    uint32_t and_count_;
};
```

Good implementation:
```cpp
void Synthesizer::synthesize(kitty::dynamic_truth_table const& tt) {
    decompose(tt);
    optimize_and_gates();
}
```

**What stays in headers:**
- Template classes and functions (C++ requirement)
- `constexpr` and `inline constexpr` functions
- One-line trivial methods: `T get() const { return member_; }`
- Default/deleted special members: `~Class() = default;`

**What moves to .cpp:**
- Functions with loops or conditionals
- Functions with more than one statement
- Operator overloads with logic
- Static factory methods
- String formatting methods (e.g., `to_string()`)

### Function Length and Readability
- **Maximum function length**: Functions should not exceed 50 lines of code
- **Extract helper functions**: When a function grows too long, extract logical blocks into well-named helper functions
- **Single responsibility**: Each function should do one thing and do it well
- **Easy to review**: Code should be structured so that each function can be reviewed in isolation
- **Nested anonymous namespaces**: Use anonymous namespaces for file-local helper functions
- **State structs**: For complex algorithms, group related state into a struct with methods instead of passing many parameters

### Code Reuse
- **Always reuse existing code**: Before implementing new functionality, search for similar code in the codebase
- **Refactor to share**: If code in one file can be useful elsewhere, move it to a shared location
- **Move freely**: Do not hesitate to relocate functions between hpp/cpp files to enable sharing
- **Consolidate duplicates**: When adding features, identify and merge duplicate logic into common utilities
- **Preferred locations for shared code**:
  - `src/lut-synth/` for fundamental utilities (e.g., `string-util.hpp`, `truth-table.hpp`)
  - `src/lut-synth/synthesis/` for synthesis-related helpers
  - Create new shared files when functionality spans multiple modules
- **Never duplicate**: If you find yourself copying code, extract it to a shared location instead

### Struct Organization
- **Semantic grouping**: Organize large structs (>15 fields) into nested sub-structs by semantic category
- **Naming convention**: Use descriptive names for sub-structs that convey purpose
- **Flat vs nested**: Use flat structure for small structs (<10 fields), nested structure for large ones

### C++ Standards
- Follow C++17 standard (as specified in CMakeLists.txt)
- Use modern C++ features: range-based loops, smart pointers when needed
- Prefer `nullptr` over `NULL`
- Use `#pragma once` for header guards

### Avoiding `auto`
- **Avoid `auto`** for variable declarations. Use explicit types for readability.
- **Allowed exceptions**:
  1. Structured bindings: `auto [a, b] = get_pair();`
  2. Lambda declarations: `auto fn = [](int x) { return x; };`
  3. Range-based for loops: `for (auto const& item : container)`
  4. `if`-init with casts: `if (auto p = dynamic_pointer_cast<T>(x))`

### Formatting
- Follow existing code style in the project
- Use consistent indentation (spaces, not tabs)
- Keep line length reasonable (aim for < 100 characters)

## Header Documentation

Use Doxygen-style comments in `.hpp` files. Use `/*! ... */` block style with backslash commands. Choose the appropriate verbosity level based on function importance and complexity.

### Verbosity Levels

#### Level 1: Minimal (simple utilities)

One-line brief only. Use for: trivial getters/setters, simple arithmetic, obvious operations.

```cpp
/*! \brief Return number of AND gates. */
uint32_t and_count() const { return and_count_; }
```

#### Level 2: Standard (regular functions)

Brief + short explanation of purpose/behavior. Optional parameter descriptions if not obvious from name/type.

```cpp
/*! \brief Synthesize XAG network from truth table.
 *
 *  Converts the given truth table into an XAG network minimizing AND gates.
 *  Supports don't-care conditions via the care set parameter.
 */
void synthesize(kitty::dynamic_truth_table const& tt, kitty::dynamic_truth_table const& care);
```

#### Level 3: Detailed (key interfaces and complex algorithms)

Brief + detailed explanation + algorithm steps + usage context.

```cpp
/*! \brief Find exact minimum AND-count synthesis via SAT.
 *
 *  Encodes the synthesis problem as a SAT instance and iteratively
 *  searches for solutions with decreasing AND gate count.
 *
 *  Algorithm:
 *  1. Encode truth table constraints as CNF clauses
 *  2. Add cardinality constraint on AND gates
 *  3. Solve and extract XAG network from model
 *  4. Decrease bound and repeat until UNSAT
 *
 *  Practical for functions with n <= 6 inputs.
 */
xag_network exact_synthesis(kitty::dynamic_truth_table const& tt);
```

### General Guidelines
- **Brief**: One sentence, starts with verb (Compute, Apply, Get, Return)
- **No redundancy**: Do not repeat function/parameter name in description
- **Skip obvious**: Do not document self-explanatory parameters
- **Active voice**: Use action verbs

## Testing

Tests are in the `test/` directory using Catch2 framework.

### Testing Rules
- Maximize coverage with minimal test cases
- Each test case should cover a distinct code path or edge case
- Prefer parameterized tests to reduce redundancy
- Function names in code can use underscores: `test_function_name_scenario`
- No redundant tests that cover the same logic
- Focus on boundary conditions and critical paths

## Development Workflow

### Verify After Implementation
- After implementing any new feature or making changes, always build and test to verify correctness
- Do not consider a task complete until the code compiles and runs successfully
- If tests fail or errors occur, fix them immediately before moving on
- Build: `cmake -B build -DBUILD_TESTS=ON && cmake --build build -j8`
- Run tests: `ctest --test-dir build --output-on-failure`
- Iterate until all tests pass and the feature works as expected
