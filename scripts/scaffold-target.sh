#!/usr/bin/env bash
set -eu

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <name>" >&2
  exit 1
fi

name="$1"
out_dir="${name}-target"

case "$name" in
  *[!a-zA-Z0-9_-]*|'')
    echo "invalid name: use [a-zA-Z0-9_-]" >&2
    exit 1
    ;;
esac

mkdir -p "$out_dir"

cat > "$out_dir/target.cpp" <<CPP
#include <nuperf/nuperf-target.h>
#include <cstdlib>
#include <cstring>

namespace {

nuperf_status_t create(const nuperf_target_t* target, nuperf_target_instance_t** out_instance) {
    if (!target || !out_instance) return NUPERF_ERR_INVALID_ARGUMENT;
    auto* inst = static_cast<nuperf_target_instance_t*>(std::calloc(1, sizeof(*inst)));
    if (!inst) return NUPERF_ERR_OUT_OF_MEMORY;
    inst->target = target;
    inst->impl = nullptr;
    *out_instance = inst;
    return NUPERF_OK;
}

void destroy(nuperf_target_instance_t* instance) {
    std::free(instance);
}

nuperf_status_t set_option(nuperf_target_instance_t*, const char*, const char*) {
    return NUPERF_ERR_NOT_SUPPORTED;
}

nuperf_status_t get_option(const nuperf_target_instance_t*, const char*, char*, size_t*) {
    return NUPERF_ERR_NOT_SUPPORTED;
}

nuperf_status_t emit(nuperf_target_instance_t*, const nuperf_hash_result_t*, nuperf_emit_sink_t*) {
    return NUPERF_ERR_NOT_SUPPORTED;
}

const nuperf_target_t g_target = {
    "${name}",
    "Scaffold target",
    NUPERF_TARGET_FLAG_TEXT_OUTPUT,
    "txt",
    nullptr,
    0,
    create,
    destroy,
    set_option,
    get_option,
    emit
};

} // namespace

extern "C" const nuperf_target_t* nuperf_target_plugin(void) {
    return &g_target;
}
CPP

cat > "$out_dir/Makefile" <<'MK'
CXX ?= c++
CXXFLAGS ?= -O2 -fPIC -std=c++17 -Wall -Wextra -pedantic
NUPERF_INCLUDE ?= /usr/local/include
NUPERF_TARGETS_DIR ?= $(HOME)/.nuperf/targets

NAME ?= changeme
LIB := libnuperf-$(NAME).so

all: $(LIB)

$(LIB): target.cpp
	$(CXX) $(CXXFLAGS) -shared $< -I$(NUPERF_INCLUDE) -o $@

install: $(LIB)
	mkdir -p $(NUPERF_TARGETS_DIR)
	cp $(LIB) $(NUPERF_TARGETS_DIR)/

clean:
	rm -f $(LIB)
MK

sed -i "s/NAME ?= changeme/NAME ?= ${name}/" "$out_dir/Makefile"

echo "created $out_dir"
echo "next: cd $out_dir && make && make install"
