#!/usr/bin/env bash
set -eu

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <name>" >&2
  exit 1
fi

name="$1"
out_dir="${name}-method"

case "$name" in
  *[!a-zA-Z0-9_-]*|'')
    echo "invalid name: use [a-zA-Z0-9_-]" >&2
    exit 1
    ;;
esac

mkdir -p "$out_dir"

cat > "$out_dir/method.cpp" <<CPP
#include <nuperf/nuperf-method.h>
#include <cstdlib>
#include <cstring>

namespace {

nuperf_status_t create(const nuperf_method_t* method, nuperf_method_instance_t** out_instance) {
    if (!method || !out_instance) return NUPERF_ERR_INVALID_ARGUMENT;
    auto* inst = static_cast<nuperf_method_instance_t*>(std::calloc(1, sizeof(*inst)));
    if (!inst) return NUPERF_ERR_OUT_OF_MEMORY;
    inst->method = method;
    inst->impl = nullptr;
    *out_instance = inst;
    return NUPERF_OK;
}

void destroy(nuperf_method_instance_t* instance) {
    std::free(instance);
}

nuperf_status_t set_option(nuperf_method_instance_t*, const char*, const char*) {
    return NUPERF_ERR_NOT_SUPPORTED;
}

nuperf_status_t get_option(const nuperf_method_instance_t*, const char*, char*, size_t*) {
    return NUPERF_ERR_NOT_SUPPORTED;
}

nuperf_status_t build(nuperf_method_instance_t*, const nuperf_keyset_t*, nuperf_hash_result_t**) {
    return NUPERF_ERR_NOT_SUPPORTED;
}

void destroy_result(nuperf_hash_result_t*) {
}

const nuperf_method_t g_method = {
    "${name}",
    "Scaffold method",
    NUPERF_METHOD_FLAG_NONE,
    nullptr,
    nullptr,
    0,
    create,
    destroy,
    set_option,
    get_option,
    build,
    destroy_result
};

} // namespace

extern "C" const nuperf_method_t* nuperf_method_plugin(void) {
    return &g_method;
}
CPP

cat > "$out_dir/Makefile" <<'MK'
CXX ?= c++
CXXFLAGS ?= -O2 -fPIC -std=c++17 -Wall -Wextra -pedantic
NUPERF_INCLUDE ?= /usr/local/include
NUPERF_METHODS_DIR ?= $(HOME)/.nuperf/methods

NAME ?= changeme
LIB := libnuperf-$(NAME).so

all: $(LIB)

$(LIB): method.cpp
	$(CXX) $(CXXFLAGS) -shared $< -I$(NUPERF_INCLUDE) -o $@

install: $(LIB)
	mkdir -p $(NUPERF_METHODS_DIR)
	cp $(LIB) $(NUPERF_METHODS_DIR)/

clean:
	rm -f $(LIB)
MK

sed -i "s/NAME ?= changeme/NAME ?= ${name}/" "$out_dir/Makefile"

echo "created $out_dir"
echo "next: cd $out_dir && make && make install"
