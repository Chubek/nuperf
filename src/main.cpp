#include <nuperf/nuperf-api.h>
#include <nuperf/nuperf-method.h>
#include <nuperf/nuperf-target.h>

#include <CLI/CLI.hpp>
#include <dynalo/dynalo.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

nuperf_status_t add_key(nuperf_keyset_t *keyset, const std::string &line) {
    if (line.empty()) {
        return NUPERF_OK;
    }
    return nuperf_keyset_add_string(keyset, line.c_str());
}

int cmd_build(const std::string &input_path,
              const std::string &output_path,
              const std::string &method,
              const std::string &target,
              const std::vector<std::string> &options) {
    nuperf_keyset_t *keyset = nullptr;
    nuperf_table_t *table = nullptr;

    nuperf_status_t st = nuperf_keyset_create(&keyset);
    if (st != NUPERF_OK) {
        std::cerr << "keyset_create failed: " << nuperf_strerror(st) << "\n";
        return 1;
    }

    st = nuperf_table_create(&table);
    if (st != NUPERF_OK) {
        std::cerr << "table_create failed: " << nuperf_strerror(st) << "\n";
        nuperf_keyset_destroy(keyset);
        return 1;
    }

    auto cleanup = [&]() {
        nuperf_table_destroy(table);
        nuperf_keyset_destroy(keyset);
    };

    std::istream *in = &std::cin;
    std::ifstream input_file;
    if (!input_path.empty() && input_path != "-") {
        input_file.open(input_path);
        if (!input_file) {
            std::cerr << "failed to open input file: " << input_path << "\n";
            cleanup();
            return 1;
        }
        in = &input_file;
    }

    std::string line;
    while (std::getline(*in, line)) {
        st = add_key(keyset, line);
        if (st != NUPERF_OK) {
            std::cerr << "failed adding key: " << nuperf_strerror(st) << "\n";
            cleanup();
            return 1;
        }
    }

    st = nuperf_table_set_keyset(table, keyset);
    if (st != NUPERF_OK) {
        std::cerr << "set_keyset failed: " << nuperf_strerror(st) << "\n";
        cleanup();
        return 1;
    }

    if (!method.empty()) {
        st = nuperf_table_set_method(table, method.c_str());
        if (st != NUPERF_OK) {
            std::cerr << "set_method failed: " << nuperf_strerror(st) << "\n";
            cleanup();
            return 1;
        }
    }

    if (!target.empty()) {
        st = nuperf_table_set_target(table, target.c_str());
        if (st != NUPERF_OK) {
            std::cerr << "set_target failed: " << nuperf_strerror(st) << "\n";
            cleanup();
            return 1;
        }
    }

    for (const std::string &opt : options) {
        const auto pos = opt.find('=');
        if (pos == std::string::npos) {
            std::cerr << "invalid option format (expected key=value): " << opt << "\n";
            cleanup();
            return 1;
        }
        st = nuperf_table_set_option(table, opt.substr(0, pos).c_str(), opt.substr(pos + 1).c_str());
        if (st != NUPERF_OK) {
            std::cerr << "set_option failed for '" << opt << "': " << nuperf_strerror(st) << "\n";
            cleanup();
            return 1;
        }
    }

    st = nuperf_table_build(table);
    if (st != NUPERF_OK) {
        std::cerr << "build failed: " << nuperf_strerror(st) << "\n";
        cleanup();
        return 1;
    }

    if (output_path.empty() || output_path == "-") {
        size_t size = 0;
        st = nuperf_table_emit_buffer(table, nullptr, &size);
        if (st != NUPERF_OK && st != NUPERF_ERR_BUFFER_TOO_SMALL) {
            std::cerr << "emit size query failed: " << nuperf_strerror(st) << "\n";
            cleanup();
            return 1;
        }
        std::string out(size, '\0');
        st = nuperf_table_emit_buffer(table, out.data(), &size);
        if (st != NUPERF_OK) {
            std::cerr << "emit buffer failed: " << nuperf_strerror(st) << "\n";
            cleanup();
            return 1;
        }
        std::cout.write(out.data(), static_cast<std::streamsize>(size));
    } else {
        st = nuperf_table_emit_file(table, output_path.c_str());
        if (st != NUPERF_OK) {
            std::cerr << "emit file failed: " << nuperf_strerror(st) << "\n";
            cleanup();
            return 1;
        }
    }

    cleanup();
    return 0;
}

std::filesystem::path nuperf_home_dir() {
    const char *home = std::getenv("HOME");
    if (home && *home) {
        return std::filesystem::path(home) / ".nuperf";
    }
    return {};
}

using MethodFactorySig = const nuperf_method_t *();
using TargetFactorySig = const nuperf_target_t *();

std::vector<std::unique_ptr<dynalo::library>> g_loaded_plugin_handles;

bool matches_plugin_name_scheme(const std::filesystem::path &path, std::string *out_name) {
    if (path.extension() != ".so") {
        return false;
    }
    const std::string stem = path.stem().string();
    static const std::string prefix = "libnuperf-";
    if (stem.rfind(prefix, 0) != 0 || stem.size() <= prefix.size()) {
        return false;
    }
    if (out_name) {
        *out_name = stem.substr(prefix.size());
    }
    return true;
}

void load_method_plugins_from_dir(const std::filesystem::path &dir) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
        return;
    }
    for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        const auto path = entry.path();
        std::string plugin_name;
        if (!matches_plugin_name_scheme(path, &plugin_name)) {
            continue;
        }
        auto lib = std::make_unique<dynalo::library>(path.string());
        try {
            auto *fn = lib->get_function<MethodFactorySig>("nuperf_method_plugin");
            if (!fn) {
                continue;
            }
            const nuperf_method_t *method = fn();
            if (method && method->name) {
                (void)nuperf_method_register(method);
                g_loaded_plugin_handles.push_back(std::move(lib));
            }
        } catch (...) {
            continue;
        }
    }
}

void load_target_plugins_from_dir(const std::filesystem::path &dir) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
        return;
    }
    for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        const auto path = entry.path();
        std::string plugin_name;
        if (!matches_plugin_name_scheme(path, &plugin_name)) {
            continue;
        }
        auto lib = std::make_unique<dynalo::library>(path.string());
        try {
            auto *fn = lib->get_function<TargetFactorySig>("nuperf_target_plugin");
            if (!fn) {
                continue;
            }
            const nuperf_target_t *target = fn();
            if (target && target->name) {
                (void)nuperf_target_register(target);
                g_loaded_plugin_handles.push_back(std::move(lib));
            }
        } catch (...) {
            continue;
        }
    }
}

} // namespace

int main(int argc, char **argv) {
    nuperf_status_t st = nuperf_init();
    if (st != NUPERF_OK) {
        std::cerr << "nuperf_init failed: " << nuperf_strerror(st) << "\n";
        return 1;
    }

    const std::filesystem::path home = nuperf_home_dir();
    load_method_plugins_from_dir(home / "methods");
    load_target_plugins_from_dir(home / "targets");

    CLI::App app{"NuPERF minimal perfect hash tool"};
    app.require_subcommand(1);

    auto *version_cmd = app.add_subcommand("version", "Print version");
    auto *methods_cmd = app.add_subcommand("list-methods", "List registered methods");
    auto *targets_cmd = app.add_subcommand("list-targets", "List registered targets");

    std::string input_path;
    std::string output_path{"-"};
    std::string method;
    std::string target;
    std::vector<std::string> options;

    auto *build_cmd = app.add_subcommand("build", "Build and emit a table");
    build_cmd->add_option("-i,--input", input_path, "Input path, '-' or omitted for stdin");
    build_cmd->add_option("-o,--output", output_path, "Output path, '-' for stdout");
    build_cmd->add_option("-m,--method", method, "Method name");
    build_cmd->add_option("-t,--target", target, "Target name");
    build_cmd->add_option("--option", options, "Method/target option in key=value form");

    CLI11_PARSE(app, argc, argv);

    int rc = 0;
    if (*version_cmd) {
        const nuperf_version_t v = nuperf_version();
        std::cout << v.major << "." << v.minor << "." << v.patch;
        if (v.suffix && *v.suffix) {
            std::cout << "-" << v.suffix;
        }
        std::cout << "\n";
    } else if (*methods_cmd) {
        std::vector<std::string> names;
        const size_t n = nuperf_method_count();
        for (size_t i = 0; i < n; ++i) {
            const char *name = nuperf_method_name(i);
            if (name) {
                names.emplace_back(name);
            }
        }
        if (names.empty()) {
            std::cerr << "no methods registered (checked ~/.nuperf/methods for libnuperf-<name>.so)\n";
            rc = 1;
        } else {
            for (const auto &name : names) {
                std::cout << name << "\n";
            }
        }
    } else if (*targets_cmd) {
        std::vector<std::string> names;
        const size_t n = nuperf_target_count();
        for (size_t i = 0; i < n; ++i) {
            const char *name = nuperf_target_name(i);
            if (name) {
                names.emplace_back(name);
            }
        }
        if (names.empty()) {
            std::cerr << "no targets registered (checked ~/.nuperf/targets for libnuperf-<name>.so)\n";
            rc = 1;
        } else {
            for (const auto &name : names) {
                std::cout << name << "\n";
            }
        }
    } else if (*build_cmd) {
        rc = cmd_build(input_path, output_path, method, target, options);
    }

    nuperf_shutdown();
    return rc;
}
