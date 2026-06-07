#include <nuperf/nuperf-api.h>
#include <nuperf/nuperf-method.h>
#include <nuperf/nuperf-target.h>

#include "cli/schema_io.hpp"

#include <Klyspec.hpp>
#include <dynalo/dynalo.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using MethodFactorySig = const nuperf_method_t *();
using TargetFactorySig = const nuperf_target_t *();

struct CommandDoc {
    std::string path;
    std::string summary;
    std::string usage;
    std::vector<std::string> examples;
    std::vector<std::pair<std::string, std::string>> options;
};

struct ParsedCommand {
    std::string command_path;
    std::vector<std::string> rest;
};

std::vector<std::unique_ptr<dynalo::library>> g_loaded_plugin_handles;

std::unordered_map<std::string, CommandDoc> make_docs() {
    std::unordered_map<std::string, CommandDoc> docs;
    docs.emplace("version", CommandDoc{
                                 "version",
                                 "Print NuPERF version.",
                                 "nuperf version",
                                 {"nuperf version"},
                                 {}});
    docs.emplace("list-methods", CommandDoc{
                                     "list-methods",
                                     "Alias for `registry methods list`.",
                                     "nuperf list-methods",
                                     {"nuperf list-methods"},
                                     {}});
    docs.emplace("list-targets", CommandDoc{
                                     "list-targets",
                                     "Alias for `registry targets list`.",
                                     "nuperf list-targets",
                                     {"nuperf list-targets"},
                                     {}});
    docs.emplace("build", CommandDoc{
                               "build",
                               "Alias for `build run`.",
                               "nuperf build [options]",
                               {"nuperf build --input keys.txt"},
                               {}});
    docs.emplace("registry methods list",
                 CommandDoc{
                     "registry methods list",
                     "List registered method plugins.",
                     "nuperf registry methods list",
                     {"nuperf registry methods list"},
                     {}});
    docs.emplace("registry targets list",
                 CommandDoc{
                     "registry targets list",
                     "List registered target plugins.",
                     "nuperf registry targets list",
                     {"nuperf registry targets list"},
                     {}});
    docs.emplace("build run", CommandDoc{
                               "build run",
                               "Build a hash table from key input.",
                               "nuperf build run [--input FILE] [--output FILE] [--method NAME] [--target NAME] [--option KEY=VALUE ...]",
                               {"nuperf build run --input keys.txt --output table.h",
                                "nuperf build run --input keys.txt --method BBHash --target stddef",
                                "cat keys.txt | nuperf build run --method stdmeth --target stddef"},
                               {{"--input, -i FILE", "Read newline keys from file (`-` for stdin)."},
                                {"--output, -o FILE", "Write output (`-` for stdout). Default: -."},
                                {"--method, -m NAME", "Method plugin name."},
                                {"--target, -t NAME", "Target plugin name."},
                                {"--option KEY=VALUE", "Repeatable table option."},
                                {"--schema FILE", "Schema file for structured imports."},
                                {"--from-json FILE", "Structured build request in JSON."},
                                {"--from-yaml FILE", "Structured build request in YAML."},
                                {"--from-sexpr FILE", "Structured build request in S-expression."},
                                {"--from-xml FILE", "Structured build request in XML."},
                                {"--dump-json-schema FILE", "Write canonical request schema."}}});
    docs.emplace("project init", CommandDoc{
                                 "project init",
                                 "Initialize a NuPERF project scaffold.",
                                 "nuperf project init [DIR] [--add-build FILE ...]",
                                 {"nuperf project init .",
                                  "nuperf project init myproj --add-build CMakeLists.txt"},
                                 {{"DIR", "Workspace directory. Default: current directory."},
                                  {"--add-build FILE", "Repeatable. Supported: CMakeLists.txt, Makefile."}}});
    docs.emplace("help", CommandDoc{
                              "help",
                              "Show global, group, or command help.",
                              "nuperf help [PATH...]",
                              {"nuperf help",
                               "nuperf help registry",
                               "nuperf help registry methods",
                               "nuperf help registry methods list",
                               "nuperf help build run"},
                              {}});
    return docs;
}

const std::unordered_map<std::string, CommandDoc> &docs() {
    static const auto d = make_docs();
    return d;
}

std::vector<std::string> split_words(const std::string &path) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : path) {
        if (c == ' ') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

std::string join_words(const std::vector<std::string> &words, std::size_t count) {
    std::string out;
    for (std::size_t i = 0; i < count; ++i) {
        if (i) out.push_back(' ');
        out += words[i];
    }
    return out;
}

std::string join_words(const std::vector<std::string> &words) {
    return join_words(words, words.size());
}

void print_global_help() {
    std::cout << "NuPERF CLI\n";
    std::cout << "Usage:\n";
    std::cout << "  nuperf <group|command> [subcommand] [options]\n";
    std::cout << "  nuperf help [PATH...]\n";
    std::cout << "\nGroups:\n";
    std::cout << "  registry   Method/target discovery.\n";
    std::cout << "  build      Table build operations.\n";
    std::cout << "  project    Workspace/project tooling.\n";
    std::cout << "\nCommands:\n";
    std::cout << "  version\n";
    std::cout << "  list-methods (alias)\n";
    std::cout << "  list-targets (alias)\n";
    std::cout << "  build (alias of build run)\n";
    std::cout << "  help\n";
    std::cout << "\nRun `nuperf help <path>` for multi-level help.\n";
}

void print_group_help(const std::string &group) {
    if (group == "registry") {
        std::cout << "Group: registry\n";
        std::cout << "Usage:\n";
        std::cout << "  nuperf registry methods list\n";
        std::cout << "  nuperf registry targets list\n";
        std::cout << "\nRun `nuperf help registry methods` or `nuperf help registry targets`.\n";
        return;
    }
    if (group == "registry methods") {
        std::cout << "Group: registry methods\n";
        std::cout << "Usage:\n";
        std::cout << "  nuperf registry methods list\n";
        return;
    }
    if (group == "registry targets") {
        std::cout << "Group: registry targets\n";
        std::cout << "Usage:\n";
        std::cout << "  nuperf registry targets list\n";
        return;
    }
    if (group == "build") {
        std::cout << "Group: build\n";
        std::cout << "Usage:\n";
        std::cout << "  nuperf build run [options]\n";
        std::cout << "\nRun `nuperf help build run`.\n";
        return;
    }
    if (group == "project") {
        std::cout << "Group: project\n";
        std::cout << "Usage:\n";
        std::cout << "  nuperf project init [DIR] [--add-build FILE ...]\n";
        return;
    }
    std::cerr << "unknown help path: " << group << "\n";
}

void print_command_help(const CommandDoc &doc) {
    std::cout << "Command: " << doc.path << "\n";
    std::cout << doc.summary << "\n";
    std::cout << "Usage:\n";
    std::cout << "  " << doc.usage << "\n";
    if (!doc.options.empty()) {
        std::cout << "Options:\n";
        for (const auto &o : doc.options) {
            std::cout << "  " << o.first << "\n";
            std::cout << "    " << o.second << "\n";
        }
    }
    if (!doc.examples.empty()) {
        std::cout << "Examples:\n";
        for (const auto &e : doc.examples) std::cout << "  " << e << "\n";
    }
}

void print_help_path(const std::vector<std::string> &path_tokens) {
    if (path_tokens.empty()) {
        print_global_help();
        return;
    }
    const std::string path = join_words(path_tokens);
    auto it = docs().find(path);
    if (it != docs().end()) {
        print_command_help(it->second);
        return;
    }
    print_group_help(path);
}

bool matches_plugin_name_scheme(const std::filesystem::path &path) {
    if (path.extension() != ".so") return false;
    const std::string stem = path.stem().string();
    static const std::string prefix = "libnuperf-";
    return stem.rfind(prefix, 0) == 0 && stem.size() > prefix.size();
}

std::filesystem::path nuperf_home_dir() {
    const char *home = std::getenv("HOME");
    if (!home || !*home) return {};
    return std::filesystem::path(home) / ".nuperf";
}

void load_method_plugins_from_dir(const std::filesystem::path &dir) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) return;
    for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec || !entry.is_regular_file()) continue;
        if (!matches_plugin_name_scheme(entry.path())) continue;
        auto lib = std::make_unique<dynalo::library>(entry.path().string());
        try {
            auto *plugin_sym = lib->get_function<MethodFactorySig>("nuperf_method_plugin");
            if (plugin_sym) {
                const nuperf_method_t *m = plugin_sym();
                if (m && m->name && nuperf_method_register(m) == NUPERF_OK) {
                    g_loaded_plugin_handles.push_back(std::move(lib));
                    continue;
                }
            }
        } catch (...) {}
        try {
            auto *descriptor_sym = lib->get_function<MethodFactorySig>("nuperf_method_descriptor");
            if (!descriptor_sym) continue;
            const nuperf_method_t *m = descriptor_sym();
            if (m && m->name && nuperf_method_register(m) == NUPERF_OK) {
                g_loaded_plugin_handles.push_back(std::move(lib));
            }
        } catch (...) {}
    }
}

void load_target_plugins_from_dir(const std::filesystem::path &dir) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) return;
    for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec || !entry.is_regular_file()) continue;
        if (!matches_plugin_name_scheme(entry.path())) continue;
        auto lib = std::make_unique<dynalo::library>(entry.path().string());
        try {
            auto *plugin_sym = lib->get_function<TargetFactorySig>("nuperf_target_plugin");
            if (plugin_sym) {
                const nuperf_target_t *t = plugin_sym();
                if (t && t->name && nuperf_target_register(t) == NUPERF_OK) {
                    g_loaded_plugin_handles.push_back(std::move(lib));
                    continue;
                }
            }
        } catch (...) {}
        try {
            auto *descriptor_sym = lib->get_function<TargetFactorySig>("nuperf_target_descriptor");
            if (!descriptor_sym) continue;
            const nuperf_target_t *t = descriptor_sym();
            if (t && t->name && nuperf_target_register(t) == NUPERF_OK) {
                g_loaded_plugin_handles.push_back(std::move(lib));
            }
        } catch (...) {}
    }
}

int cmd_build_run(const std::string &input_path, const std::string &output_path, const std::string &method,
                  const std::string &target, const std::vector<std::string> &options,
                  nuperf::cli::InputFormat input_format, const std::string &schema_path,
                  const std::string &schema_dump_path) {
    nuperf_keyset_t *keyset = nullptr;
    nuperf_table_t *table = nullptr;

    nuperf_status_t st = nuperf_keyset_create(&keyset);
    if (st != NUPERF_OK) {
        std::cerr << "keyset_create: " << nuperf_strerror(st) << "\n";
        return 1;
    }

    st = nuperf_table_create(&table);
    if (st != NUPERF_OK) {
        std::cerr << "table_create: " << nuperf_strerror(st) << "\n";
        nuperf_keyset_destroy(keyset);
        return 1;
    }

    auto cleanup = [&]() {
        nuperf_table_destroy(table);
        nuperf_keyset_destroy(keyset);
    };

    if (!schema_dump_path.empty()) {
        std::string error;
        if (!nuperf::cli::dump_dataset_schema(schema_dump_path, error)) {
            std::cerr << "dump-json-schema: " << error << "\n";
            cleanup();
            return 1;
        }
    }

    std::string effective_method = method.empty() ? "stdmeth" : method;
    std::string effective_target = target.empty() ? "stddef" : target;
    std::vector<std::string> effective_options = options;

    if (input_format == nuperf::cli::InputFormat::lines) {
        std::istream *in = &std::cin;
        std::ifstream input_file;
        if (!input_path.empty() && input_path != "-") {
            input_file.open(input_path);
            if (!input_file) {
                std::cerr << "failed to open input: " << input_path << "\n";
                cleanup();
                return 1;
            }
            in = &input_file;
        }

        std::string line;
        while (std::getline(*in, line)) {
            if (line.empty()) continue;
            st = nuperf_keyset_add_string(keyset, line.c_str());
            if (st != NUPERF_OK) {
                std::cerr << "add_string: " << nuperf_strerror(st) << "\n";
                cleanup();
                return 1;
            }
        }
    } else {
        if (input_path.empty() || input_path == "-") {
            std::cerr << "structured imports require a file path\n";
            cleanup();
            return 1;
        }
        nuperf::cli::BuildRequest request;
        std::string error;
        if (!nuperf::cli::load_build_request(input_path, input_format, schema_path, request, error)) {
            std::cerr << "structured import failed: " << error << "\n";
            cleanup();
            return 1;
        }
        for (const std::string &key : request.keys) {
            st = nuperf_keyset_add_string(keyset, key.c_str());
            if (st != NUPERF_OK) {
                std::cerr << "add_string: " << nuperf_strerror(st) << "\n";
                cleanup();
                return 1;
            }
        }
        if (effective_method.empty()) effective_method = request.method;
        if (effective_target.empty()) effective_target = request.target;
        effective_options.insert(effective_options.end(), request.options.begin(), request.options.end());
    }

    if ((st = nuperf_table_set_keyset(table, keyset)) != NUPERF_OK) {
        std::cerr << "set_keyset: " << nuperf_strerror(st) << "\n";
        cleanup();
        return 1;
    }
    if (!effective_method.empty() && (st = nuperf_table_set_method(table, effective_method.c_str())) != NUPERF_OK) {
        std::cerr << "set_method: " << nuperf_strerror(st) << "\n";
        cleanup();
        return 1;
    }
    if (!effective_target.empty() && (st = nuperf_table_set_target(table, effective_target.c_str())) != NUPERF_OK) {
        std::cerr << "set_target: " << nuperf_strerror(st) << "\n";
        cleanup();
        return 1;
    }

    for (const std::string &opt : effective_options) {
        const auto pos = opt.find('=');
        if (pos == std::string::npos) {
            std::cerr << "invalid --option, expected key=value: " << opt << "\n";
            cleanup();
            return 1;
        }
        st = nuperf_table_set_option(table, opt.substr(0, pos).c_str(), opt.substr(pos + 1).c_str());
        if (st != NUPERF_OK) {
            std::cerr << "set_option(" << opt << "): " << nuperf_strerror(st) << "\n";
            cleanup();
            return 1;
        }
    }

    if ((st = nuperf_table_build(table)) != NUPERF_OK) {
        std::cerr << "build: " << nuperf_strerror(st) << "\n";
        cleanup();
        return 1;
    }

    if (output_path.empty() || output_path == "-") {
        size_t size = 0;
        st = nuperf_table_emit_buffer(table, nullptr, &size);
        if (st != NUPERF_OK && st != NUPERF_ERR_BUFFER_TOO_SMALL) {
            std::cerr << "emit size query: " << nuperf_strerror(st) << "\n";
            cleanup();
            return 1;
        }
        std::string out(size, '\0');
        st = nuperf_table_emit_buffer(table, out.data(), &size);
        if (st != NUPERF_OK) {
            std::cerr << "emit buffer: " << nuperf_strerror(st) << "\n";
            cleanup();
            return 1;
        }
        std::cout.write(out.data(), static_cast<std::streamsize>(size));
    } else {
        st = nuperf_table_emit_file(table, output_path.c_str());
        if (st != NUPERF_OK) {
            std::cerr << "emit file: " << nuperf_strerror(st) << "\n";
            cleanup();
            return 1;
        }
    }

    cleanup();
    return 0;
}

int cmd_project_init(const std::string &dir, const std::vector<std::string> &build_files) {
    const std::filesystem::path root =
        dir.empty() ? std::filesystem::current_path() : std::filesystem::path(dir);
    std::error_code ec;
    std::filesystem::create_directories(root / "keys", ec);
    std::filesystem::create_directories(root / "out", ec);
    std::filesystem::create_directories(root / "scripts", ec);

    std::ofstream sample(root / "keys" / "keys.txt", std::ios::trunc);
    sample << "alpha\nbeta\ngamma\n";

    std::ofstream lua(root / "scripts" / "build.lua", std::ios::trunc);
    lua << "-- lnuperf is preloaded by NuPERF Lua runner\n"
           "local ks = nuperf.new_keyset()\n"
           "ks:add_string('alpha')\n"
           "ks:add_string('beta')\n"
           "local t = nuperf.new_table()\n"
           "t:set_method('stdmeth')\n"
           "t:set_target('stddef')\n"
           "t:build(ks)\n"
           "t:emit_file('out/table.h')\n";

    for (const std::string &bf : build_files) {
        if (bf == "CMakeLists.txt") {
            std::ofstream cm(root / "CMakeLists.txt", std::ios::trunc);
            cm << "cmake_minimum_required(VERSION 3.16)\n"
                  "project(nuperf_user_project C CXX)\n"
                  "find_package(nuperf REQUIRED)\n"
                  "add_executable(app main.c)\n"
                  "target_link_libraries(app PRIVATE nuperf::nuperf)\n";
        } else if (bf == "Makefile") {
            std::ofstream mk(root / "Makefile", std::ios::trunc);
            mk << "CC ?= cc\n"
                  "CFLAGS += -I/usr/local/include\n"
                  "LDFLAGS += -L/usr/local/lib -lnuperf\n"
                  "all: app\n"
                  "app: main.c\n\t$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)\n";
        } else {
            std::cerr << "unsupported --add-build value: " << bf << "\n";
            return 1;
        }
    }

    std::cout << "Initialized NuPERF workspace at " << root.string() << "\n";
    return 0;
}

ParsedCommand resolve_command_path(int argc, char **argv) {
    ParsedCommand out;
    if (argc < 2) return out;

    std::vector<std::string> tokens;
    for (int i = 1; i < argc; ++i) tokens.emplace_back(argv[i]);

    std::size_t best_len = 0;
    for (const auto &entry : docs()) {
        if (entry.first == "help") continue;
        const auto words = split_words(entry.first);
        if (words.empty() || words.size() > tokens.size()) continue;
        bool match = true;
        for (std::size_t i = 0; i < words.size(); ++i) {
            if (tokens[i] != words[i]) {
                match = false;
                break;
            }
        }
        if (match && words.size() > best_len) {
            best_len = words.size();
            out.command_path = entry.first;
        }
    }

    if (best_len == 0) {
        out.command_path = tokens[0];
        out.rest.assign(tokens.begin() + 1, tokens.end());
    } else {
        out.rest.assign(tokens.begin() + static_cast<long>(best_len), tokens.end());
    }
    return out;
}

bool has_help_flag(const std::vector<std::string> &args) {
    for (const auto &a : args) if (a == "--help" || a == "-h") return true;
    return false;
}

} // namespace

int main(int argc, char **argv) {
    if (nuperf_init() != NUPERF_OK) {
        std::cerr << "nuperf_init failed\n";
        return 1;
    }

    const auto home = nuperf_home_dir();
    load_method_plugins_from_dir(home / "methods");
    load_target_plugins_from_dir(home / "targets");

    if (argc < 2) {
        print_global_help();
        nuperf_shutdown();
        return 0;
    }

    if (std::string(argv[1]) == "help") {
        std::vector<std::string> help_path;
        for (int i = 2; i < argc; ++i) help_path.emplace_back(argv[i]);
        print_help_path(help_path);
        nuperf_shutdown();
        return 0;
    }

    ParsedCommand parsed_command = resolve_command_path(argc, argv);
    if (parsed_command.command_path == "--help" || parsed_command.command_path == "-h") {
        print_global_help();
        nuperf_shutdown();
        return 0;
    }
    if (has_help_flag(parsed_command.rest)) {
        auto it = docs().find(parsed_command.command_path);
        if (it != docs().end()) {
            print_command_help(it->second);
            nuperf_shutdown();
            return 0;
        }
    }

    int rc = 0;
    if (parsed_command.command_path == "version") {
        if (!parsed_command.rest.empty()) {
            std::cerr << "unexpected positional argument(s)\n";
            nuperf_shutdown();
            return 1;
        }
        const auto v = nuperf_version();
        std::cout << v.major << "." << v.minor << "." << v.patch;
        if (v.suffix && *v.suffix) std::cout << "-" << v.suffix;
        std::cout << "\n";
    } else if (parsed_command.command_path == "registry methods list" || parsed_command.command_path == "list-methods") {
        if (!parsed_command.rest.empty()) {
            std::cerr << "unexpected positional argument(s)\n";
            nuperf_shutdown();
            return 1;
        }
        for (size_t i = 0; i < nuperf_method_count(); ++i) {
            if (const nuperf_method_t *m = nuperf_method_at(i)) {
                if (m->name) std::cout << m->name;
                if (m->description && *m->description) std::cout << "\t" << m->description;
                std::cout << "\n";
            }
        }
    } else if (parsed_command.command_path == "registry targets list" || parsed_command.command_path == "list-targets") {
        if (!parsed_command.rest.empty()) {
            std::cerr << "unexpected positional argument(s)\n";
            nuperf_shutdown();
            return 1;
        }
        for (size_t i = 0; i < nuperf_target_count(); ++i) {
            if (const nuperf_target_t *t = nuperf_target_at(i)) {
                if (t->name) std::cout << t->name;
                if (t->description && *t->description) std::cout << "\t" << t->description;
                std::cout << "\n";
            }
        }
    } else if (parsed_command.command_path == "build run" || parsed_command.command_path == "build") {
        klyspec::Registry reg;
        reg.register_command({"build-run", "build command"});

        klyspec::OptionSpec input;
        input.id = "input";
        input.names = {"--input", "-i"};
        reg.register_argument("build-run", input);

        klyspec::OptionSpec output;
        output.id = "output";
        output.names = {"--output", "-o"};
        output.default_value = "-";
        reg.register_argument("build-run", output);

        klyspec::OptionSpec method;
        method.id = "method";
        method.names = {"--method", "-m"};
        reg.register_argument("build-run", method);

        klyspec::OptionSpec target;
        target.id = "target";
        target.names = {"--target", "-t"};
        reg.register_argument("build-run", target);

        klyspec::OptionSpec schema;
        schema.id = "schema";
        schema.names = {"--schema"};
        reg.register_argument("build-run", schema);

        klyspec::OptionSpec from_json;
        from_json.id = "from-json";
        from_json.names = {"--from-json"};
        reg.register_argument("build-run", from_json);

        klyspec::OptionSpec from_yaml;
        from_yaml.id = "from-yaml";
        from_yaml.names = {"--from-yaml"};
        reg.register_argument("build-run", from_yaml);

        klyspec::OptionSpec from_sexpr;
        from_sexpr.id = "from-sexpr";
        from_sexpr.names = {"--from-sexpr"};
        reg.register_argument("build-run", from_sexpr);

        klyspec::OptionSpec from_xml;
        from_xml.id = "from-xml";
        from_xml.names = {"--from-xml"};
        reg.register_argument("build-run", from_xml);

        klyspec::OptionSpec dump_schema;
        dump_schema.id = "dump-json-schema";
        dump_schema.names = {"--dump-json-schema"};
        reg.register_argument("build-run", dump_schema);

        klyspec::ArgumentSpec option;
        option.id = "option";
        option.kind = klyspec::ArgumentKind::repeatable;
        option.value_policy = klyspec::ValuePolicy::required;
        option.names = {"--option"};
        reg.register_argument("build-run", option);

        klyspec::KlyCLIService cli(reg);
        const auto pr = cli.parse("build-run", parsed_command.rest);
        if (!pr.ok) {
            for (const auto &d : pr.diagnostics) std::cerr << d << "\n";
            nuperf_shutdown();
            return 1;
        }
        if (!pr.positionals.empty()) {
            std::cerr << "unexpected positional argument(s)\n";
            nuperf_shutdown();
            return 1;
        }

        auto get1 = [&](const char *key) -> std::string {
            auto it = pr.values.find(key);
            return (it != pr.values.end() && !it->second.empty()) ? it->second.front() : std::string{};
        };

        nuperf::cli::InputFormat input_format = nuperf::cli::InputFormat::lines;
        std::string input_path = get1("input");
        const std::string from_json_v = get1("from-json");
        const std::string from_yaml_v = get1("from-yaml");
        const std::string from_sexpr_v = get1("from-sexpr");
        const std::string from_xml_v = get1("from-xml");
        std::size_t structured_count = 0;

        if (!from_json_v.empty()) {
            input_format = nuperf::cli::InputFormat::json;
            input_path = from_json_v;
            ++structured_count;
        }
        if (!from_yaml_v.empty()) {
            input_format = nuperf::cli::InputFormat::yaml;
            input_path = from_yaml_v;
            ++structured_count;
        }
        if (!from_sexpr_v.empty()) {
            input_format = nuperf::cli::InputFormat::sexpr;
            input_path = from_sexpr_v;
            ++structured_count;
        }
        if (!from_xml_v.empty()) {
            input_format = nuperf::cli::InputFormat::xml;
            input_path = from_xml_v;
            ++structured_count;
        }
        if (structured_count > 1) {
            std::cerr << "use only one of --from-json/--from-yaml/--from-sexpr/--from-xml\n";
            nuperf_shutdown();
            return 1;
        }

        std::vector<std::string> opts;
        auto it = pr.values.find("option");
        if (it != pr.values.end()) opts = it->second;
        rc = cmd_build_run(input_path, get1("output"), get1("method"), get1("target"), opts, input_format,
                           get1("schema"), get1("dump-json-schema"));
    } else if (parsed_command.command_path == "project init") {
        klyspec::Registry reg;
        reg.register_command({"project-init", "project init"});

        klyspec::ArgumentSpec add_build;
        add_build.id = "add-build";
        add_build.kind = klyspec::ArgumentKind::repeatable;
        add_build.value_policy = klyspec::ValuePolicy::required;
        add_build.names = {"--add-build"};
        reg.register_argument("project-init", add_build);

        klyspec::KlyCLIService cli(reg);
        const auto pr = cli.parse("project-init", parsed_command.rest);
        if (!pr.ok) {
            for (const auto &d : pr.diagnostics) std::cerr << d << "\n";
            nuperf_shutdown();
            return 1;
        }
        if (pr.positionals.size() > 1) {
            std::cerr << "unexpected extra positional argument(s)\n";
            nuperf_shutdown();
            return 1;
        }
        const std::string dir = pr.positionals.empty() ? std::string{} : pr.positionals.front();
        std::vector<std::string> builds;
        auto it = pr.values.find("add-build");
        if (it != pr.values.end()) builds = it->second;
        rc = cmd_project_init(dir, builds);
    } else {
        std::cerr << "unknown command: " << parsed_command.command_path << "\n";
        std::cerr << "run `nuperf help` for command hierarchy\n";
        nuperf_shutdown();
        return 1;
    }

    nuperf_shutdown();
    return rc;
}
