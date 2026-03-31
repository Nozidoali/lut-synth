---
name: review
description: Review code for adherence to project standards and anti-patterns
---

# Review Command

Review the selected code or file for adherence to project standards.

## Coding Style
- **No comments in cpp files**: Code should be self-explanatory through clear naming
- **Short and clean**: Concise, readable code
- **No try-catch blocks**: Use assertions instead
- **Minimal error handling**: Use `assert()` for invariants
- **Minimal print messages**: Avoid debug prints and verbose logging
- **No special characters**: Only alphanumeric and underscores in code

## File Naming
- Lowercase with hyphens: `feature-name.cpp`, `feature-name.hpp`
- No underscores in file names
- No abbreviations: use full words
- Proper directory placement (`third-party/` for CMake submodule projects)

## Header/Implementation Separation
- Headers contain declarations, documentation, and trivial one-liners only
- All function bodies with loops, conditionals, or multiple statements must be in .cpp files
- Templates and `constexpr` functions are exceptions (stay in headers)
- Check for implementations that should be moved out of headers

## Function Length and Readability
- Functions must not exceed 50 lines of code
- Each function should have a single responsibility
- Identify functions that should be split into helpers
- Use anonymous namespaces for file-local helpers
- Use state structs for complex algorithms instead of many parameters

## Header Documentation
- Check Doxygen comments in .hpp files match the appropriate verbosity level:
  - **Minimal**: Trivial one-liners, getters/setters
  - **Standard**: Internal functions with non-trivial logic
  - **Detailed**: Public API, core algorithms, mathematical functions

## Code Reuse
- Check for duplicated logic that should be extracted to shared location
- Verify existing utilities are used where applicable
- Flag copy-pasted code that should be consolidated

## C++ Standards
- C++17 features used correctly
- `#pragma once` for header guards
- `const` and `constexpr` used where appropriate
- Range-based for loops preferred over index-based
- Explicit types preferred over `auto` (except structured bindings, lambdas, range-for)

## Anti-Pattern Checks

1. **Unsafe `operator[]` on `unordered_map`**: Creates default entries (value 0) silently, causing wrong assignments. Always use `.find()` + assert:
   ```cpp
   auto it = map.find(key);
   assert(it != map.end());
   use(it->second);
   ```

Provide specific suggestions for improvements.
