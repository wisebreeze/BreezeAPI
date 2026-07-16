# BreezeAPI — Interface Reference

BreezeAPI is a lightweight C++ hooking library for Android, built on top of [Dobby](https://github.com/jmpews/Dobby). It provides both a C++ class interface and a C-compatible ABI for inline hooking, symbol resolution, and hook lifecycle management.

Compiled output: `libbreeze_api.so` (arm64-v8a)

---

## Table of Contents

- [Quick Start](#quick-start)
- [C++ API](#c-api)
  - [BreezeAPI (Singleton)](#breezeapi-singleton)
  - [Lifecycle](#lifecycle)
  - [Hook Operations](#hook-operations)
  - [Symbol Resolution](#symbol-resolution)
  - [Query](#query)
  - [Configuration](#configuration)
  - [Types](#types)
- [C ABI](#c-abi)
- [Build](#build)
- [Integration](#integration)

---

## Quick Start

```cpp
#include <breeze_api.h>

// Original function pointer
static void* (*orig_GameTick)(void* self, int dt) = nullptr;

// Replacement function
void* hook_GameTick(void* self, int dt) {
    // Your custom logic here
    return orig_GameTick(self, dt);
}

// Install hook
void install_hooks() {
    breeze::BreezeAPI::Instance().Init();

    breeze::BreezeAPI::Instance().HookBySymbol(
        "libminecraftpe.so",
        "_ZN6Server4tickEi",
        reinterpret_cast<void*>(hook_GameTick),
        reinterpret_cast<void**>(&orig_GameTick),
        "ServerTick"
    );
}
```

---

## C++ API

### BreezeAPI (Singleton)

```cpp
namespace breeze {
class BREEZE_EXPORT BreezeAPI {
public:
    static BreezeAPI& Instance();
    // ...
};
}
```

Singleton pattern. Access the API through `BreezeAPI::Instance()`.

---

### Lifecycle

#### `bool Init(const BreezeConfig& config = {})`

Initialize the API. Must be called before any hook operations. Safe to call multiple times; subsequent calls are no-ops.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `config` | `BreezeConfig` | Optional configuration (log level, tag, etc.) |

**Returns:** `true` on success.

---

#### `void Shutdown()`

Shut down the API. Unhooks all active hooks and releases resources.

---

#### `bool IsInitialized() const`

Query initialization state.

---

### Hook Operations

#### `HookStatus HookBySymbol(library, symbol, replace, original, tag)`

Hook a function by its exported symbol name inside a loaded library.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `library` | `const std::string&` | Library name (e.g. `"libminecraftpe.so"`) |
| `symbol` | `const std::string&` | Mangled or demangled exported symbol |
| `replace` | `Address` (void*) | Replacement function pointer |
| `original` | `Address*` (void**) | [out] Receives the trampoline for calling the original |
| `tag` | `const std::string&` | Optional human-readable tag for debugging |

**Returns:** `HookStatus`

---

#### `HookStatus HookByOffset(library, offset, replace, original, tag)`

Hook a function by offset from the library base address.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `library` | `const std::string&` | Library name |
| `offset` | `uintptr_t` | Offset from library base |
| `replace` | `Address` | Replacement function pointer |
| `original` | `Address*` | [out] Trampoline |
| `tag` | `const std::string&` | Optional tag |

**Returns:** `HookStatus`

---

#### `HookStatus HookByAddress(target, replace, original, tag)`

Hook a function by absolute address.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `target` | `Address` | Absolute target address |
| `replace` | `Address` | Replacement function pointer |
| `original` | `Address*` | [out] Trampoline |
| `tag` | `const std::string&` | Optional tag |

**Returns:** `HookStatus`

---

#### `HookStatus Unhook(Address target)`

Remove a previously installed hook.

**Returns:** `HookStatus::Ok` on success, `HookStatus::NotHooked` if the address was not hooked.

---

#### `void UnhookAll()`

Remove all active hooks.

---

### Symbol Resolution

#### `Address ResolveLibraryBase(const std::string& library) const`

Resolve the base load address of a library by reading `/proc/self/maps`.

**Returns:** Base address, or `nullptr` if not found.

---

#### `Address ResolveSymbol(const std::string& library, const std::string& symbol) const`

Resolve an exported symbol address. Resolution strategy:
1. `dlsym(RTLD_DEFAULT, symbol)` — search all loaded libraries
2. `dlopen(library) + dlsym` — search specific library
3. `DobbySymbolResolver` — Dobby's internal resolver

**Returns:** Symbol address, or `nullptr` if not found.

---

### Query

#### `std::optional<HookInfo> GetHookInfo(Address target) const`

Get information about a hook by its target address.

---

#### `std::optional<HookInfo> GetHookInfoByTag(const std::string& tag) const`

Get information about a hook by its tag name.

---

#### `std::vector<HookInfo> GetAllHooks() const`

List all currently active hooks.

---

### Configuration

#### `void SetLogLevel(LogLevel level)` / `LogLevel GetLogLevel() const`

Set or get the current log level. Log messages below this level are suppressed.

---

### Types

#### `HookStatus`

```cpp
enum class HookStatus : int32_t {
    Ok              = 0,  // Hook installed/removed successfully
    AlreadyHooked   = 1,  // Address is already hooked
    NotHooked       = 2,  // Address is not currently hooked
    SymbolNotFound  = 3,  // Symbol or library could not be resolved
    InvalidAddress  = 4,  // Null or invalid address
    InternalError   = 5,  // Dobby internal error
};
```

#### `LogLevel`

```cpp
enum class LogLevel : int32_t {
    Verbose = 0,
    Debug   = 1,
    Info    = 2,   // Default
    Warn    = 3,
    Error   = 4,
    Silent  = 5,
};
```

#### `HookInfo`

```cpp
struct HookInfo {
    std::string  name;           // Tag / identifier
    std::string  library;        // Source library name
    std::string  symbol;         // Source symbol or offset expression
    Address      target_addr;    // Hooked address
    Address      trampoline;     // Original function trampoline
    HookStatus   status;         // Current status
};
```

#### `BreezeConfig`

```cpp
struct BreezeConfig {
    LogLevel     log_level    = LogLevel::Info;
    bool         log_to_android = true;     // Use __android_log_print
    std::string  log_tag      = "BreezeAPI";
};
```

---

## C ABI

For use from JNI or other C-compatible consumers. All functions are exported from `libbreeze_api.so`.

| Function | Signature | Description |
|----------|-----------|-------------|
| `breeze_init` | `bool breeze_init()` | Initialize the API |
| `breeze_shutdown` | `void breeze_shutdown()` | Shut down and unhook all |
| `breeze_hook_symbol` | `int32_t breeze_hook_symbol(const char* lib, const char* sym, void* replace, void** original)` | Hook by symbol name |
| `breeze_hook_offset` | `int32_t breeze_hook_offset(const char* lib, uintptr_t offset, void* replace, void** original)` | Hook by library offset |
| `breeze_hook_address` | `int32_t breeze_hook_address(void* target, void* replace, void** original)` | Hook by absolute address |
| `breeze_unhook` | `int32_t breeze_unhook(void* target)` | Unhook a specific address |
| `breeze_unhook_all` | `void breeze_unhook_all()` | Unhook everything |
| `breeze_resolve_library` | `void* breeze_resolve_library(const char* lib)` | Resolve library base address |
| `breeze_resolve_symbol` | `void* breeze_resolve_symbol(const char* lib, const char* sym)` | Resolve symbol address |
| `breeze_set_log_level` | `void breeze_set_log_level(int32_t level)` | Set log level |

Return values for hook functions correspond to `HookStatus` enum values.

---

## Build

### Prerequisites

- Android NDK r25c+ (set `ANDROID_NDK` environment variable)
- CMake 3.22+
- Clang with C++20 support

### Build command

```bash
# Set NDK path
export ANDROID_NDK=/path/to/android-ndk

# Build for arm64-v8a
./build.sh
```

### Manual CMake

```bash
cmake -B build/arm64-v8a \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-26 \
    -DCMAKE_BUILD_TYPE=Release \
    -S .

cmake --build build/arm64-v8a -j$(nproc)
```

### Output

```
build/arm64-v8a/out/lib/arm64-v8a/libbreeze_api.so
```

---

## Integration

### As a shared library (recommended)

1. Copy `libbreeze_api.so` to your app's `jniLibs/arm64-v8a/`
2. Add to your `CMakeLists.txt`:

```cmake
add_library(breeze_api SHARED IMPORTED)
set_target_properties(breeze_api PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../jniLibs/arm64-v8a/libbreeze_api.so
    INTERFACE_INCLUDE_DIRECTORIES ${BREEZE_API_INCLUDE_DIR}
)
target_link_libraries(your_lib breeze_api log dl)
```

3. Load in Java/Kotlin:

```kotlin
System.loadLibrary("breeze_api")
```

### As a subproject

Add BreezeAPI as a git submodule and include it in your CMake build:

```cmake
add_subdirectory(path/to/BreezeAPI breeze_api)
target_link_libraries(your_lib breeze_api)
```

---

## License

MIT License — see [LICENSE](../LICENSE) for details.

Dobby is licensed under the Apache License 2.0.
