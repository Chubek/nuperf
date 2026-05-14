# NuPERF Target Plugins

## ABI contract

- Shared object name: `libnuperf-<name>.so`.
- Install location: `~/.nuperf/targets/`.
- Required export symbol: `nuperf_target_plugin`.
- Export type: `const nuperf_target_t* (*)()`.

## Minimal target implementation

```cpp
#include <nuperf/nuperf-target.h>

static nuperf_status_t create(const nuperf_target_t* t, nuperf_target_instance_t** out) { /* ... */ }
static void destroy(nuperf_target_instance_t* i) { /* ... */ }
static nuperf_status_t set_option(nuperf_target_instance_t* i, const char* k, const char* v) { /* ... */ }
static nuperf_status_t get_option(const nuperf_target_instance_t* i, const char* k, char* b, size_t* n) { /* ... */ }
static nuperf_status_t emit(nuperf_target_instance_t* i, const nuperf_hash_result_t* r, nuperf_emit_sink_t* s) { /* ... */ }

static const nuperf_target_t g_target = {
    "example", "Example target", NUPERF_TARGET_FLAG_TEXT_OUTPUT,
    "txt", nullptr, 0,
    create, destroy, set_option, get_option, emit
};

extern "C" const nuperf_target_t* nuperf_target_plugin(void) {
    return &g_target;
}
```

## Build flags

```sh
c++ -std=c++17 -fPIC -shared target.cpp -I/path/to/nuperf/include -o libnuperf-example.so
```

## Install plugin

```sh
mkdir -p ~/.nuperf/targets
cp libnuperf-example.so ~/.nuperf/targets/
```

## Validate

```sh
nuperf list-targets
```

Expected: `example` appears in the list.
