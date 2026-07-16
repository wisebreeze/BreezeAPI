# BreezeAPI

A lightweight C++ inline hooking library for Android, built on [Dobby](https://github.com/jmpews/Dobby). Provides both a C++ class interface and a C-compatible ABI for function hooking, symbol resolution, and hook lifecycle management.

## Features

- **Three hook modes** — by symbol name, by library offset, or by absolute address
- **Automatic symbol resolution** — dlsym + DobbySymbolResolver fallback chain
- **Hook lifecycle management** — install, query, and remove hooks with tag-based indexing
- **C + C++ API** — use the object-oriented C++ interface or the flat C ABI from JNI
- **Android logging** — built-in `__android_log_print` integration with configurable log levels
- **Minimal footprint** — compiles to a single `libbreeze_api.so`

## Quick Start

```cpp
#include <breeze_api.h>

static void* (*orig_GameTick)(void*, int) = nullptr;

void* hook_GameTick(void* self, int dt) {
    // custom logic
    return orig_GameTick(self, dt);
}

void install() {
    breeze::BreezeAPI::Instance().Init();
    breeze::BreezeAPI::Instance().HookBySymbol(
        "libminecraftpe.so",
        "_ZN6Server4tickEi",
        (void*)hook_GameTick,
        (void**)&orig_GameTick,
        "ServerTick"
    );
}
```

## Documentation

- [API Reference](docs/API.md)

## Build

```bash
export ANDROID_NDK=/path/to/ndk
./build.sh
```

Output: `build/arm64-v8a/out/lib/arm64-v8a/libbreeze_api.so`

## Project Structure

```
BreezeAPI/
├── CMakeLists.txt          # Root CMake build
├── build.sh                # Android build script
├── include/                # Public headers
│   ├── breeze_api.h        # Core API + C ABI
│   ├── hook_manager.h      # Hook management
│   ├── symbol_resolver.h   # Symbol resolution
│   └── logger.h            # Logging utility
├── src/                    # Implementation
│   ├── breeze_api.cpp
│   ├── hook_manager.cpp
│   ├── symbol_resolver.cpp
│   └── logger.cpp
├── docs/
│   └── API.md              # Interface documentation
└── third_party/
    └── dobby/              # Dobby (git submodule)
```

## License

MIT
