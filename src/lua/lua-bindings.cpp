// src/lua/lua-bindings.cpp

#include "nuperf/nuperf-api.h"
#include "nuperf/nuperf-types.h"

#include "../cli/schema_io.hpp"

#include <sol/sol.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace nuperf::lua_bindings {

namespace {

inline void throw_on_error(nuperf_status_t st) {
    if (st != NUPERF_OK) {
        throw sol::error(nuperf_strerror(st));
    }
}

class LuaKeyset {
public:
    LuaKeyset() {
        throw_on_error(nuperf_keyset_create(&ks_));
    }

    ~LuaKeyset() {
        if (ks_) {
            nuperf_keyset_destroy(ks_);
            ks_ = nullptr;
        }
    }

    LuaKeyset(const LuaKeyset&) = delete;
    LuaKeyset& operator=(const LuaKeyset&) = delete;

    LuaKeyset(LuaKeyset&& other) noexcept : ks_(other.ks_) {
        other.ks_ = nullptr;
    }

    LuaKeyset& operator=(LuaKeyset&& other) noexcept {
        if (this != &other) {
            if (ks_) {
                nuperf_keyset_destroy(ks_);
            }
            ks_ = other.ks_;
            other.ks_ = nullptr;
        }
        return *this;
    }

    void clear() {
        nuperf_keyset_clear(ks_);
    }

    void add_string(const std::string& s) {
        throw_on_error(nuperf_keyset_add_string(ks_, s.c_str()));
    }

    void add_binary(const sol::object& obj) {
        if (obj.is<std::string>()) {
            const std::string s = obj.as<std::string>();
            throw_on_error(
                nuperf_keyset_add_binary(
                    ks_,
                    reinterpret_cast<const uint8_t*>(s.data()),
                    s.size()));
            return;
        }

        if (obj.get_type() == sol::type::table) {
            sol::table t = obj.as<sol::table>();
            std::vector<uint8_t> bytes;
            bytes.reserve(t.size());
            for (std::size_t i = 1; i <= t.size(); ++i) {
                bytes.push_back(static_cast<uint8_t>(t.get<int>(i)));
            }
            throw_on_error(
                nuperf_keyset_add_binary(
                    ks_,
                    bytes.data(),
                    bytes.size()));
            return;
        }

        throw sol::error("add_binary expects a string or array-like table of bytes");
    }

    void add_u32(uint32_t v) {
        throw_on_error(nuperf_keyset_add_uint32(ks_, v));
    }

    void add_u64(uint64_t v) {
        throw_on_error(nuperf_keyset_add_uint64(ks_, v));
    }

    std::size_t size() const {
        return nuperf_keyset_size(ks_);
    }

    int key_type() const {
        return static_cast<int>(nuperf_keyset_type(ks_));
    }

    nuperf_keyset_t* get() const {
        return ks_;
    }

private:
    nuperf_keyset_t* ks_ = nullptr;
};

class LuaTable {
public:
    LuaTable() {
        throw_on_error(nuperf_table_create(&tbl_));
    }

    ~LuaTable() {
        if (tbl_) {
            nuperf_table_destroy(tbl_);
            tbl_ = nullptr;
        }
    }

    LuaTable(const LuaTable&) = delete;
    LuaTable& operator=(const LuaTable&) = delete;

    LuaTable(LuaTable&& other) noexcept : tbl_(other.tbl_) {
        other.tbl_ = nullptr;
    }

    LuaTable& operator=(LuaTable&& other) noexcept {
        if (this != &other) {
            if (tbl_) {
                nuperf_table_destroy(tbl_);
            }
            tbl_ = other.tbl_;
            other.tbl_ = nullptr;
        }
        return *this;
    }

    void set_method(const std::string& name) {
        throw_on_error(nuperf_table_set_method(tbl_, name.c_str()));
    }

    void set_target(const std::string& name) {
        throw_on_error(nuperf_table_set_target(tbl_, name.c_str()));
    }

    void set_option(const std::string& name, const std::string& value) {
        throw_on_error(nuperf_table_set_option(tbl_, name.c_str(), value.c_str()));
    }

    std::string get_option(const std::string& name) const {
        std::size_t size = 0;
        nuperf_status_t st = nuperf_table_get_option(tbl_, name.c_str(), nullptr, &size);
        if (st != NUPERF_ERR_BUFFER_TOO_SMALL && st != NUPERF_OK) {
            throw_on_error(st);
        }

        std::string out(size ? size - 1 : 0, '\0');
        if (size == 0) {
            return out;
        }

        st = nuperf_table_get_option(tbl_, name.c_str(), out.data(), &size);
        throw_on_error(st);
        return out;
    }

    void build(const LuaKeyset& ks) {
        throw_on_error(nuperf_table_set_keyset(tbl_, ks.get()));
        throw_on_error(nuperf_table_build(tbl_));
    }

    std::string emit_buffer() {
        std::size_t size = 0;
        nuperf_status_t st = nuperf_table_emit_buffer(tbl_, nullptr, &size);
        if (st != NUPERF_ERR_BUFFER_TOO_SMALL && st != NUPERF_OK) {
            throw_on_error(st);
        }

        std::string out(size, '\0');
        if (size == 0) {
            return out;
        }

        st = nuperf_table_emit_buffer(tbl_, out.data(), &size);
        throw_on_error(st);
        out.resize(size);
        return out;
    }

    void emit_file(const std::string& path) {
        throw_on_error(nuperf_table_emit_file(tbl_, path.c_str()));
    }

    sol::table build_stats(sol::this_state ts) const {
        sol::state_view lua(ts);
        sol::table t = lua.create_table();

        nuperf_build_stats_t stats{};
        if (nuperf_table_get_stats(tbl_, &stats) != NUPERF_OK) {
            return t;
        }

        t["key_count"] = stats.key_count;
        t["build_time_us"] = stats.build_time_us;
        t["build_memory_bytes"] = stats.build_memory_bytes;
        t["table_size_bytes"] = stats.table_size_bytes;
        t["bits_per_key"] = stats.bits_per_key;
        t["method_stats"] = stats.method_stats ? std::string(stats.method_stats) : std::string{};
        return t;
    }

    nuperf_table_t* get() const {
        return tbl_;
    }

private:
    nuperf_table_t* tbl_ = nullptr;
};

static std::vector<std::string> enumerate_methods() {
    std::vector<std::string> out;
    const std::size_t n = nuperf_method_count();
    out.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        const char* name = nuperf_method_name(i);
        if (name != nullptr) {
            out.emplace_back(name);
        }
    }
    return out;
}

static std::vector<std::string> enumerate_targets() {
    std::vector<std::string> out;
    const std::size_t n = nuperf_target_count();
    out.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        const char* name = nuperf_target_name(i);
        if (name != nullptr) {
            out.emplace_back(name);
        }
    }
    return out;
}

static sol::table version_table(sol::this_state ts) {
    sol::state_view lua(ts);
    sol::table t = lua.create_table();

    const nuperf_version_t v = nuperf_version();
    t["major"] = v.major;
    t["minor"] = v.minor;
    t["patch"] = v.patch;
    t["suffix"] = v.suffix ? std::string(v.suffix) : std::string{};
    return t;
}

static sol::table schema_namespace(sol::this_state ts) {
    sol::state_view lua(ts);
    sol::table schema = lua.create_table();

    schema["dataset_json"] = []() {
        return nuperf::cli::canonical_dataset_schema_json();
    };
    schema["dump_json"] = [](const std::string &path) {
        std::string error;
        if (!nuperf::cli::dump_dataset_schema(path, error)) {
            throw sol::error(error);
        }
    };
    schema["load"] = [&lua](const std::string &path, const std::string &format_name, const sol::optional<std::string> &schema_path) {
        nuperf::cli::InputFormat format = nuperf::cli::InputFormat::json;
        if (format_name == "json") format = nuperf::cli::InputFormat::json;
        else if (format_name == "yaml") format = nuperf::cli::InputFormat::yaml;
        else if (format_name == "sexpr") format = nuperf::cli::InputFormat::sexpr;
        else if (format_name == "xml") format = nuperf::cli::InputFormat::xml;
        else throw sol::error("unsupported schema format");

        nuperf::cli::BuildRequest request;
        std::string error;
        if (!nuperf::cli::load_build_request(path, format, schema_path ? std::filesystem::path(*schema_path) : std::filesystem::path{}, request, error)) {
            throw sol::error(error);
        }

        sol::table out = lua.create_table();
        sol::table keys = lua.create_table(static_cast<int>(request.keys.size()), 0);
        for (std::size_t index = 0; index < request.keys.size(); ++index) {
            keys[index + 1] = request.keys[index];
        }
        sol::table options = lua.create_table();
        for (const std::string &entry : request.options) {
            const std::size_t pos = entry.find('=');
            if (pos != std::string::npos) {
                options[entry.substr(0, pos)] = entry.substr(pos + 1);
            }
        }
        out["keys"] = keys;
        out["method"] = request.method;
        out["target"] = request.target;
        out["options"] = options;
        return out;
    };

    return schema;
}

} // namespace

void bind(sol::state_view lua) {
    sol::table nu = lua.create_table();

    nu["version"] = &version_table;
    nu["strerror"] = [](int code) {
        return std::string(nuperf_strerror(static_cast<nuperf_status_t>(code)));
    };
    nu["method_count"] = &nuperf_method_count;
    nu["target_count"] = &nuperf_target_count;
    nu["methods"] = &enumerate_methods;
    nu["targets"] = &enumerate_targets;

    nu["status"] = lua.create_table_with(
        "OK", static_cast<int>(NUPERF_OK),
        "ERR_INVALID_ARGUMENT", static_cast<int>(NUPERF_ERR_INVALID_ARGUMENT),
        "ERR_INVALID_STATE", static_cast<int>(NUPERF_ERR_INVALID_STATE),
        "ERR_OUT_OF_MEMORY", static_cast<int>(NUPERF_ERR_OUT_OF_MEMORY),
        "ERR_NOT_FOUND", static_cast<int>(NUPERF_ERR_NOT_FOUND),
        "ERR_BUFFER_TOO_SMALL", static_cast<int>(NUPERF_ERR_BUFFER_TOO_SMALL)
    );

    nu["key_type"] = lua.create_table_with(
        "UNKNOWN", static_cast<int>(NUPERF_KEY_TYPE_UNKNOWN),
        "STRING", static_cast<int>(NUPERF_KEY_TYPE_STRING),
        "BINARY", static_cast<int>(NUPERF_KEY_TYPE_BINARY),
        "U32", static_cast<int>(NUPERF_KEY_TYPE_UINT32),
        "U64", static_cast<int>(NUPERF_KEY_TYPE_UINT64)
    );

    lua.new_usertype<LuaKeyset>(
        "NuperfKeyset",
        sol::constructors<LuaKeyset()>(),
        "clear", &LuaKeyset::clear,
        "add_string", &LuaKeyset::add_string,
        "add_binary", &LuaKeyset::add_binary,
        "add_u32", &LuaKeyset::add_u32,
        "add_u64", &LuaKeyset::add_u64,
        "size", &LuaKeyset::size,
        "key_type", &LuaKeyset::key_type
    );

    lua.new_usertype<LuaTable>(
        "NuperfTable",
        sol::constructors<LuaTable()>(),
        "set_method", &LuaTable::set_method,
        "set_target", &LuaTable::set_target,
        "set_option", &LuaTable::set_option,
        "get_option", &LuaTable::get_option,
        "build", &LuaTable::build,
        "emit_buffer", &LuaTable::emit_buffer,
        "emit_file", &LuaTable::emit_file,
        "build_stats", &LuaTable::build_stats
    );

    nu["new_keyset"] = []() {
        return LuaKeyset{};
    };

    nu["new_table"] = []() {
        return LuaTable{};
    };
    nu["schema"] = schema_namespace(lua.lua_state());

    lua["nuperf"] = nu;
}

sol::state create_state() {
    sol::state lua;
    lua.open_libraries(
        sol::lib::base,
        sol::lib::package,
        sol::lib::string,
        sol::lib::table,
        sol::lib::math,
        sol::lib::os
    );
    bind(sol::state_view(lua));
    lua["lnuperf"] = lua["nuperf"];
    return lua;
}

sol::protected_function_result run_script(
    sol::state_view lua,
    const std::string& code,
    const std::string& chunk_name) {
    sol::load_result loaded = lua.load(code);
    if (!loaded.valid()) {
        sol::error err = loaded;
        throw sol::error(std::string(chunk_name) + ": " + err.what());
    }

    sol::protected_function pf = loaded;
    return pf();
}

sol::protected_function_result run_file(
    sol::state_view lua,
    const std::string& path) {
    return lua.safe_script_file(path, &sol::script_pass_on_error);
}

} // namespace nuperf::lua_bindings
