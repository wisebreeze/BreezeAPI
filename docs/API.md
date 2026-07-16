# BreezeAPI — Interface Reference

BreezeAPI is a lightweight C++ hooking and JavaScript scripting library for Android, built on top of [Dobby](https://github.com/jmpews/Dobby) and [QuickJS](https://bellard.org/quickjs/). It provides both a C++ class interface and a C-compatible ABI for inline hooking, symbol resolution, JavaScript execution, and hook lifecycle management.

Compiled output: `libbreeze_api.so` (arm64-v8a, armeabi-v7a, x86_64)

---

## Table of Contents

- [Quick Start](#quick-start)
- [C++ API](#c-api)
  - [BreezeAPI (Singleton)](#breezeapi-singleton)
  - [Lifecycle](#lifecycle)
  - [Hook Operations](#hook-operations)
  - [Symbol Resolution](#symbol-resolution)
  - [JavaScript Engine](#javascript-engine)
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

// Install hook and run JavaScript
void install_hooks() {
    auto& api = breeze::BreezeAPI::Instance();
    api.Init();

    // Hook a native function
    api.HookBySymbol(
        "libminecraftpe.so",
        "_ZN6Server4tickEi",
        reinterpret_cast<void*>(hook_GameTick),
        reinterpret_cast<void**>(&orig_GameTick),
        "ServerTick"
    );

    // Register a native function callable from JS
    api.RegisterJSFunction("getTickRate", [](const std::vector<std::string>& args) {
        return std::to_string(20);  // return 20 TPS
    });

    // Evaluate JavaScript
    auto result = api.EvalJS(R"(
        var rate = getTickRate();
        "Tick rate: " + rate;
    )");

    if (result.success) {
        // result.value == "Tick rate: 20"
    }
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

Initialize the API. Must be called before any hook or JS operations. Safe to call multiple times; subsequent calls are no-ops.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `config` | `BreezeConfig` | Optional configuration (log level, tag, etc.) |

**Returns:** `true` on success.

---

#### `void Shutdown()`

Shut down the API. Unhooks all active hooks, releases the JS engine, and frees resources.

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

### JavaScript Engine

BreezeAPI embeds [QuickJS](https://bellard.org/quickjs/) (MIT license), providing a full ECMAScript 2024 compliant JavaScript engine. The JS engine is initialized automatically with `BreezeAPI::Init()` and supports:

- Full JavaScript evaluation (ES2024)
- ES module import/export
- Native C++ function registration as JS globals
- Global property get/set (string, number, boolean)
- Custom module registration for `import` from native code
- Memory limits and garbage collection control

---

#### `JSResult EvalJS(const std::string& source, const std::string& filename = "<eval>")`

Evaluate a JavaScript source string.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `source` | `const std::string&` | JavaScript code to evaluate |
| `filename` | `const std::string&` | Optional filename for error reporting |

**Returns:** `JSResult` with `success`, `type`, and `value` (stringified result or error message).

```cpp
auto r = api.EvalJS("1 + 2");
// r.success == true, r.type == JSType::Number, r.value == "3"
```

---

#### `JSResult EvalJSModule(const std::string& source, const std::string& filename = "<module>.mjs")`

Evaluate a JavaScript ES module from source. Supports `import`/`export` syntax.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `source` | `const std::string&` | JavaScript module code |
| `filename` | `const std::string&` | Module filename (should end with `.mjs`) |

**Returns:** `JSResult`

```cpp
auto r = api.EvalJSModule(R"(
    export function greet(name) { return "Hello, " + name; }
    greet("World");
)", "greet.mjs");
```

---

#### `bool SetJSGlobalString(const std::string& name, const std::string& value)`
#### `bool SetJSGlobalNumber(const std::string& name, double value)`
#### `bool SetJSGlobalBool(const std::string& name, bool value)`

Set a global JavaScript property with the specified value.

**Returns:** `true` on success.

```cpp
api.SetJSGlobalString("modName", "BreezeMod");
api.SetJSGlobalNumber("version", 1.1);
api.SetJSGlobalBool("enabled", true);
```

---

#### `std::optional<std::string> GetJSGlobalString(const std::string& name) const`
#### `std::optional<double> GetJSGlobalNumber(const std::string& name) const`

Get a global JavaScript property. Returns `std::nullopt` if the property does not exist or cannot be converted.

```cpp
auto name = api.GetJSGlobalString("modName");  // "BreezeMod"
auto ver  = api.GetJSGlobalNumber("version");   // 1.1
```

---

#### `bool RegisterJSFunction(const std::string& name, JSNativeCallback callback)`

Register a native C++ callback as a global JavaScript function. The callback receives arguments as stringified values and must return a string result. Arguments are automatically converted to strings before being passed; the return string becomes a JS string value.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `name` | `const std::string&` | Function name on the global object |
| `callback` | `JSNativeCallback` | C++ callback: `std::function<std::string(const std::vector<std::string>&)>` |

**Returns:** `true` on success.

```cpp
api.RegisterJSFunction("add", [](const std::vector<std::string>& args) {
    double a = std::stod(args[0]);
    double b = std::stod(args[1]);
    return std::to_string(a + b);
});

auto r = api.EvalJS("add(3, 4)");  // r.value == "7"
```

---

#### `bool UnregisterJSFunction(const std::string& name)`

Unregister a previously registered native function.

---

#### `bool RegisterJSModule(const std::string& specifier, const std::string& source)`

Register a JavaScript source as a module that can be imported by name.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `specifier` | `const std::string&` | Module specifier (used in `import from`) |
| `source` | `const std::string&` | JavaScript module source code |

```cpp
api.RegisterJSModule("my_utils", R"(
    export function double(x) { return x * 2; }
)");

auto r = api.EvalJSModule(R"(
    import { double } from "my_utils";
    double(21);
)", "test.mjs");
// r.value == "42"
```

---

#### `void JSGC()`

Run a garbage collection cycle on the JavaScript engine.

---

#### `size_t GetJSMemoryUsage() const`

Get current JavaScript heap memory usage in bytes.

---

#### `void SetJSMemoryLimit(size_t limit)`

Set the JavaScript heap memory limit. `0` means unlimited.

---

#### `void SetJSStackSize(size_t limit)`

Set the JavaScript stack size limit. `0` means default (256KB).

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

#### `JSResult`

```cpp
struct JSResult {
    bool        success = false;
    int32_t     type    = 0;     // JSType: 0=Undefined,1=Null,2=Bool,3=Number,4=String,5=Object,6=Function,7=Exception
    std::string value;           // Stringified result or error message
};
```

#### `JSNativeCallback`

```cpp
using JSNativeCallback = std::function<std::string(const std::vector<std::string>&)>;
```

---

## C ABI

For use from JNI or other C-compatible consumers. All functions are exported from `libbreeze_api.so`.

### Hook & Resolution

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

### JavaScript Engine

| Function | Signature | Description |
|----------|-----------|-------------|
| `breeze_js_eval` | `breeze_js_result breeze_js_eval(const char* source, const char* filename)` | Evaluate JS code |
| `breeze_js_eval_module` | `breeze_js_result breeze_js_eval_module(const char* source, const char* filename)` | Evaluate JS module |
| `breeze_js_set_global_string` | `bool breeze_js_set_global_string(const char* name, const char* value)` | Set global string |
| `breeze_js_set_global_number` | `bool breeze_js_set_global_number(const char* name, double value)` | Set global number |
| `breeze_js_set_global_bool` | `bool breeze_js_set_global_bool(const char* name, int32_t value)` | Set global boolean |
| `breeze_js_get_global_string` | `char* breeze_js_get_global_string(const char* name)` | Get global string (free with `breeze_js_free_string`) |
| `breeze_js_get_global_number` | `double breeze_js_get_global_number(const char* name)` | Get global number |
| `breeze_js_gc` | `void breeze_js_gc()` | Run garbage collection |
| `breeze_js_memory_usage` | `size_t breeze_js_memory_usage()` | Get JS heap usage |
| `breeze_js_set_memory_limit` | `void breeze_js_set_memory_limit(size_t limit)` | Set heap limit |
| `breeze_js_set_stack_size` | `void breeze_js_set_stack_size(size_t limit)` | Set stack limit |
| `breeze_js_free_string` | `void breeze_js_free_string(char* str)` | Free a string from `breeze_js_get_global_string` |

**`breeze_js_result` struct:**
```c
typedef struct breeze_js_result {
    int32_t     success;   // 1 = success, 0 = error
    int32_t     type;      // JSType value (0-7)
    const char* value;     // Stringified result or error message
} breeze_js_result;
```

**Note:** The `value` pointer in `breeze_js_result` is only valid until the next JS API call. Copy the string immediately if you need to preserve it.

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
target_link_libraries(your_lib breeze_api log dl m)
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

## Third-Party Licenses

- **Dobby** — Apache License 2.0
- **QuickJS** — MIT License (Fabrice Bellard)
