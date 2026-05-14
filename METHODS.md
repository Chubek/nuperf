# NuPERF Method Plugins

## ABI contract

- Shared object name: `libnuperf-<name>.so`.
- Install location: `~/.nuperf/methods/`.
- Required export symbol: `nuperf_method_plugin`.
- Export type: `const nuperf_method_t* (*)()`.

## Minimal method implementation

```cpp
#include <nuperf/nuperf-method.h>

static nuperf_status_t create(const nuperf_method_t* m, nuperf_method_instance_t** out) { /* ... */ }
static void destroy(nuperf_method_instance_t* i) { /* ... */ }
static nuperf_status_t set_option(nuperf_method_instance_t* i, const char* k, const char* v) { /* ... */ }
static nuperf_status_t get_option(const nuperf_method_instance_t* i, const char* k, char* b, size_t* n) { /* ... */ }
static nuperf_status_t build(nuperf_method_instance_t* i, const nuperf_keyset_t* ks, nuperf_hash_result_t** out) { /* ... */ }
static void destroy_result(nuperf_hash_result_t* r) { /* ... */ }

static const nuperf_method_t g_method = {
    "example", "Example method", NUPERF_METHOD_FLAG_MINIMAL,
    nullptr, nullptr, 0,
    create, destroy, set_option, get_option, build, destroy_result
};

extern "C" const nuperf_method_t* nuperf_method_plugin(void) {
    return &g_method;
}
```

## Build flags

- Compile as PIC shared object.
- Include NuPERF headers (`include/`).
- Link against NuPERF library if needed by your implementation.

Example:

```sh
c++ -std=c++17 -fPIC -shared method.cpp -I/path/to/nuperf/include -o libnuperf-example.so
```

## Install plugin

```sh
mkdir -p ~/.nuperf/methods
cp libnuperf-example.so ~/.nuperf/methods/
```

## Validate

```sh
nuperf list-methods
```

Expected: `example` appears in the list.
