# Contributing to BreezeAPI

Thank you for your interest in contributing to BreezeAPI. This document describes the process and conventions for contributing to this project.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Environment](#development-environment)
- [Code Style](#code-style)
- [Commit Messages](#commit-messages)
- [Pull Request Process](#pull-request-process)
- [Adding New Hooks or API](#adding-new-hooks-or-api)
- [Testing](#testing)
- [Documentation](#documentation)

## Code of Conduct

Be respectful and constructive. Personal attacks, harassment, and discriminatory language are not tolerated.

## Getting Started

1. Fork the repository
2. Clone your fork with submodules:
   ```bash
   git clone --recurse-submodules https://github.com/<your-username>/BreezeAPI.git
   ```
3. Create a feature branch:
   ```bash
   git checkout -b feature/your-feature-name
   ```
4. Make your changes
5. Submit a pull request

## Development Environment

### Required tools

- Android NDK r25c or later
- CMake 3.22+
- Clang with C++20 support
- Git with submodule support

### Building locally

```bash
export ANDROID_NDK=/path/to/ndk
./build.sh
```

For debug builds with full symbols and no stripping:

```bash
cmake -B build/debug \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-26 \
    -DCMAKE_BUILD_TYPE=debug \
    -S .
cmake --build build/debug -j$(nproc)
```

## Code Style

### C++ Style

- Use C++20 features where appropriate
- Use `snake_case` for functions, variables, and file names
- Use `PascalCase` for class and struct names
- Use `kCamelCase` for constants
- Use `ALL_CAPS_SNAKE` for preprocessor macros
- Indent with 4 spaces, no tabs
- Always use `{}` for single-statement `if`/`else`/`for`/`while` bodies
- Place opening braces on the same line
- Use `#pragma once` for header guards
- Use `nullptr` instead of `NULL` or `0`
- Use `override` and `const` wherever applicable
- Use `std::string` and STL containers over C equivalents
- Use `std::optional` for nullable return values

### Header organization

Public headers live in `include/`. Implementation details stay in `src/`. If a new component needs an internal header, place it in `src/` and do not install it.

### Naming conventions for API

- C++ namespace: `breeze`
- C ABI prefix: `breeze_`
- Exported classes: `BREEZE_EXPORT` macro
- Public methods: `PascalCase` on the class, but `snake_case` for free functions

### Example

```cpp
// include/my_component.h
#pragma once

#include "breeze_api.h"

namespace breeze {

class BREEZE_EXPORT MyComponent {
public:
    bool DoSomething(const std::string& name);
private:
    struct Impl;
    Impl* impl_;
};

} // namespace breeze
```

## Commit Messages

Follow the [Conventional Commits](https://www.conventionalcommits.org/) specification:

```
<type>(<scope>): <description>

[optional body]

[optional footer]
```

### Types

| Type | Description |
|------|-------------|
| `feat` | New feature or API |
| `fix` | Bug fix |
| `refactor` | Code restructure without behavior change |
| `docs` | Documentation changes |
| `test` | Adding or updating tests |
| `build` | Build system or CI changes |
| `ci` | CI/CD configuration |
| `chore` | Maintenance, dependency updates |

### Examples

```
feat(hook): add HookByPattern for regex-based symbol matching
fix(resolver): handle missing /proc/self/maps gracefully
docs(api): add C ABI documentation for breeze_hook_offset
build(cmake): add x86_64 ABI target
```

## Pull Request Process

1. **One logical change per PR.** Do not mix unrelated refactors with feature additions.
2. **Update documentation.** If you add or change public API, update `docs/API.md` and the relevant header comments.
3. **Ensure the build passes.** Run `./build.sh` locally before pushing. CI will verify, but local verification saves time.
4. **Write a clear PR description** that explains:
   - What the change does
   - Why it is needed
   - How it was tested
   - Any breaking changes
5. **Keep the PR small.** Under 400 lines of diff is ideal. Larger changes should be split into a series of PRs.
6. **Address review feedback** promptly and push new commits (do not force-push during review unless asked).

### PR checklist

Before submitting, verify:

- [ ] Build succeeds with `./build.sh`
- [ ] Public headers have complete Doxygen-style comments
- [ ] `docs/API.md` is updated for any API changes
- [ ] Commit messages follow Conventional Commits
- [ ] No unrelated changes in the PR
- [ ] No hardcoded paths or credentials

## Adding New Hooks or API

When adding new hook functionality:

1. Add the method declaration to `include/breeze_api.h` (C++ API)
2. Add the corresponding C ABI function in the `extern "C"` block
3. Implement the logic in `src/breeze_api.cpp`
4. If the feature is complex, create a dedicated component (header in `include/`, implementation in `src/`)
5. Update `CMakeLists.txt` if new source files are added
6. Document the new API in `docs/API.md`

## Testing

Currently BreezeAPI does not have an automated test suite (hooking libraries require a live process to test against). When contributing:

- Test your changes manually on an Android device or emulator
- Verify that the `.so` loads and the C++/C API functions work as expected
- For bug fixes, describe the reproduction steps and confirm the fix in your PR

If you want to contribute a test framework, that would be very welcome as a separate PR.

## Documentation

- API documentation lives in `docs/API.md`
- Keep header comments in sync with the docs
- Use Doxygen-style comments for public API:

```cpp
/**
 * Hook a function by its symbol name.
 *
 * @param library  Library name (e.g. "libminecraftpe.so")
 * @param symbol   Exported symbol name
 * @param replace  Replacement function pointer
 * @param original [out] Receives the trampoline
 * @param tag      Optional debug tag
 * @return HookStatus
 */
HookStatus HookBySymbol(...);
```

## Questions

If you are unsure about anything, open an issue with the "question" label before starting work. It is better to discuss an approach first than to submit a large PR that needs fundamental changes.
