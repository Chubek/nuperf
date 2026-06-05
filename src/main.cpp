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
#include <vector>

namespace {

using MethodFactorySig = const nuperf_method_t *();
using TargetFactorySig = const nuperf_target_t *();

std::vector<std::unique_ptr<dynalo::library>> g_loaded_plugin_handles;

struct HelpOptionDoc {
    std::vector<std::string> names;
    std::string value_name;
    std::string description;
    bool required{false};
    bool repeatable{false};
    std::optional<std::string> default_value;
};

struct HelpCommandDoc {
    std::string name;
    std::string description;
    std::string usage;
    std::vector<std::string> aliases;
    std::vector<HelpOptionDoc> options;
    std::vector<std::string> examples;
};

std::string join_names(const std::vector<std::string> &names) {
    std::string out;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i) out += ", ";
        out += names[i];
    }
    return out;
}

const HelpCommandDoc *find_help_command_doc(const std::vector<HelpCommandDoc> &docs, const std::string &token) {
    for (const auto &doc : docs) {
        if (doc.name == token) return &doc;
        for (const std::string &alias : doc.aliases) if (alias == token) return &doc;
    }
    return nullptr;
}

void print_global_help(const std::vector<HelpCommandDoc> &docs) {
    std::cout << "NuPERF CLI\n";
    std::cout << "Usage:\n";
    std::cout << "  nuperf <command> [options]\n";
    std::cout << "  nuperf help [command] [usage|options|examples]\n";
    std::cout << "  nuperf --help | -h\n\n";
    std::cout << "Commands:\n";
    for (const auto &doc : docs) {
        std::cout << "  " << doc.name;
        if (!doc.aliases.empty()) std::cout << " (" << join_names(doc.aliases) << ")";
        std::cout << "\n    " << doc.description << "\n";
    }
    std::cout << "\nUse `nuperf <command> --help` for command help.\n";
}

void print_help_usage_section(const HelpCommandDoc &doc) {
    std::cout << "Usage:\n";
    std::cout << "  " << doc.usage << "\n";
}

void print_help_options_section(const HelpCommandDoc &doc) {
    std::cout << "Options:\n";
    if (doc.options.empty()) {
        std::cout << "  (none)\n";
        return;
    }
    for (const auto &opt : doc.options) {
        std::cout << "  " << join_names(opt.names);
        if (!opt.value_name.empty()) std::cout << " <" << opt.value_name << ">";
        if (opt.repeatable) std::cout << " (repeatable)";
        if (opt.required) std::cout << " (required)";
        if (opt.default_value.has_value()) std::cout << " [default: " << *opt.default_value << "]";
        std::cout << "\n    " << opt.description << "\n";
    }
}

void print_help_examples_section(const HelpCommandDoc &doc) {
    std::cout << "Examples:\n";
    if (doc.examples.empty()) {
        std::cout << "  (none)\n";
        return;
    }
    for (const auto &example : doc.examples) std::cout << "  " << example << "\n";
}

void print_command_help(const HelpCommandDoc &doc, const std::string &section = {}) {
    std::cout << "Command: " << doc.name << "\n";
    std::cout << doc.description << "\n";
    if (!doc.aliases.empty()) std::cout << "Aliases: " << join_names(doc.aliases) << "\n";
    if (section.empty() || section == "usage") {
        print_help_usage_section(doc);
        if (!section.empty()) return;
    }
    if (section.empty() || section == "options") {
        print_help_options_section(doc);
        if (!section.empty()) return;
    }
    if (section.empty() || section == "examples") {
        print_help_examples_section(doc);
        if (!section.empty()) return;
    }
}

bool is_help_token(const std::string &token) {
    return token == "--help" || token == "-h";
}

std::filesystem::path nuperf_home_dir() {
    const char *home = std::getenv("HOME");
    if (home && *home) return std::filesystem::path(home) / ".nuperf";
    return {};
}

bool matches_plugin_name_scheme(const std::filesystem::path &path) {
    if (path.extension() != ".so") return false;
    const std::string stem = path.stem().string();
    static const std::string prefix = "libnuperf-";
    return stem.rfind(prefix, 0) == 0 && stem.size() > prefix.size();
}

void load_method_plugins_from_dir(const std::filesystem::path &dir) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) return;
    for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec || !entry.is_regular_file()) continue;
        if (!matches_plugin_name_scheme(entry.path())) continue;
        auto lib = std::make_unique<dynalo::library>(entry.path().string());
        try {
            auto *fn = lib->get_function<MethodFactorySig>("nuperf_method_plugin");
            if (!fn) continue;
            const nuperf_method_t *m = fn();
            if (m && m->name && nuperf_method_register(m) == NUPERF_OK) g_loaded_plugin_handles.push_back(std::move(lib));
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
            auto *fn = lib->get_function<TargetFactorySig>("nuperf_target_plugin");
            if (!fn) continue;
            const nuperf_target_t *t = fn();
            if (t && t->name && nuperf_target_register(t) == NUPERF_OK) g_loaded_plugin_handles.push_back(std::move(lib));
        } catch (...) {}
    }
}

int cmd_build(const std::string &input_path, const std::string &output_path, const std::string &method,
              const std::string &target, const std::vector<std::string> &options,
              nuperf::cli::InputFormat input_format, const std::string &schema_path,
              const std::string &schema_dump_path) {
    nuperf_keyset_t *keyset = nullptr;
    nuperf_table_t *table = nullptr;
    nuperf_status_t st = nuperf_keyset_create(&keyset);
    if (st != NUPERF_OK) return (std::cerr << "keyset_create: " << nuperf_strerror(st) << "\n", 1);
    st = nuperf_table_create(&table);
    if (st != NUPERF_OK) return (std::cerr << "table_create: " << nuperf_strerror(st) << "\n", nuperf_keyset_destroy(keyset), 1);

    auto cleanup = [&]() { nuperf_table_destroy(table); nuperf_keyset_destroy(keyset); };
    if (!schema_dump_path.empty()) {
        std::string error;
        if (!nuperf::cli::dump_dataset_schema(schema_dump_path, error)) {
            return (std::cerr << "dump-json-schema: " << error << "\n", cleanup(), 1);
        }
    }

    std::string effective_method = method;
    std::string effective_target = target;
    std::vector<std::string> effective_options = options;

    if (input_format == nuperf::cli::InputFormat::lines) {
        std::istream *in = &std::cin;
        std::ifstream input_file;
        if (!input_path.empty() && input_path != "-") {
            input_file.open(input_path);
            if (!input_file) return (std::cerr << "failed to open input: " << input_path << "\n", cleanup(), 1);
            in = &input_file;
        }

        std::string line;
        while (std::getline(*in, line)) {
            if (line.empty()) continue;
            st = nuperf_keyset_add_string(keyset, line.c_str());
            if (st != NUPERF_OK) return (std::cerr << "add_string: " << nuperf_strerror(st) << "\n", cleanup(), 1);
        }
    } else {
        if (input_path.empty() || input_path == "-") {
            return (std::cerr << "structured imports require a file path\n", cleanup(), 1);
        }
        nuperf::cli::BuildRequest request;
        std::string error;
        if (!nuperf::cli::load_build_request(input_path, input_format, schema_path, request, error)) {
            return (std::cerr << "structured import failed: " << error << "\n", cleanup(), 1);
        }
        for (const std::string &key : request.keys) {
            st = nuperf_keyset_add_string(keyset, key.c_str());
            if (st != NUPERF_OK) return (std::cerr << "add_string: " << nuperf_strerror(st) << "\n", cleanup(), 1);
        }
        if (effective_method.empty()) effective_method = request.method;
        if (effective_target.empty()) effective_target = request.target;
        effective_options.insert(effective_options.end(), request.options.begin(), request.options.end());
    }

    if ((st = nuperf_table_set_keyset(table, keyset)) != NUPERF_OK)
        return (std::cerr << "set_keyset: " << nuperf_strerror(st) << "\n", cleanup(), 1);
    if (!effective_method.empty() && (st = nuperf_table_set_method(table, effective_method.c_str())) != NUPERF_OK)
        return (std::cerr << "set_method: " << nuperf_strerror(st) << "\n", cleanup(), 1);
    if (!effective_target.empty() && (st = nuperf_table_set_target(table, effective_target.c_str())) != NUPERF_OK)
        return (std::cerr << "set_target: " << nuperf_strerror(st) << "\n", cleanup(), 1);

    for (const std::string &opt : effective_options) {
        const auto pos = opt.find('=');
        if (pos == std::string::npos)
            return (std::cerr << "invalid --option, expected key=value: " << opt << "\n", cleanup(), 1);
        st = nuperf_table_set_option(table, opt.substr(0, pos).c_str(), opt.substr(pos + 1).c_str());
        if (st != NUPERF_OK)
            return (std::cerr << "set_option(" << opt << "): " << nuperf_strerror(st) << "\n", cleanup(), 1);
    }

    if ((st = nuperf_table_build(table)) != NUPERF_OK)
        return (std::cerr << "build: " << nuperf_strerror(st) << "\n", cleanup(), 1);

    if (output_path.empty() || output_path == "-") {
        size_t size = 0;
        st = nuperf_table_emit_buffer(table, nullptr, &size);
        if (st != NUPERF_OK && st != NUPERF_ERR_BUFFER_TOO_SMALL)
            return (std::cerr << "emit size query: " << nuperf_strerror(st) << "\n", cleanup(), 1);
        std::string out(size, '\0');
        st = nuperf_table_emit_buffer(table, out.data(), &size);
        if (st != NUPERF_OK)
            return (std::cerr << "emit buffer: " << nuperf_strerror(st) << "\n", cleanup(), 1);
        std::cout.write(out.data(), static_cast<std::streamsize>(size));
    } else {
        st = nuperf_table_emit_file(table, output_path.c_str());
        if (st != NUPERF_OK)
            return (std::cerr << "emit file: " << nuperf_strerror(st) << "\n", cleanup(), 1);
    }

    cleanup();
    return 0;
}

int cmd_init(const std::string &dir, const std::vector<std::string> &build_files) {
    const std::filesystem::path root = dir.empty() ? std::filesystem::current_path() : std::filesystem::path(dir);
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
                  "# TODO: set NUPERF_DIR if needed\n"
                  "find_package(nuperf REQUIRED)\n"
                  "add_executable(app main.c)\n"
                  "target_link_libraries(app PRIVATE nuperf::nuperf)\n";
        } else if (bf == "Makefile") {
            std::ofstream mk(root / "Makefile", std::ios::trunc);
            mk << "# TODO: adjust include/lib paths for your environment\n"
                  "CC ?= cc\n"
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

} // namespace

int main(int argc, char **argv) {
    if (nuperf_init() != NUPERF_OK) return (std::cerr << "nuperf_init failed\n", 1);
    const auto home = nuperf_home_dir();
    load_method_plugins_from_dir(home / "methods");
    load_target_plugins_from_dir(home / "targets");

    klyspec::Registry reg;
    std::vector<HelpCommandDoc> help_docs;
    auto add_command = [&](const std::string &name, const std::string &description, const std::string &usage,
                           std::vector<std::string> aliases, std::vector<std::string> examples = {}) {
        reg.register_command({name, description});
        for (const auto &alias : aliases) reg.register_alias(name, alias);
        help_docs.push_back(HelpCommandDoc{name, description, usage, std::move(aliases), {}, std::move(examples)});
    };
    auto add_help_option_doc = [&](const std::string &command, HelpOptionDoc doc) {
        if (auto *entry = const_cast<HelpCommandDoc *>(find_help_command_doc(help_docs, command))) {
            entry->options.push_back(std::move(doc));
        }
    };
    auto add_help_flag = [&](const std::string &command) {
        klyspec::ArgumentSpec help;
        help.id = "help";
        help.kind = klyspec::ArgumentKind::flag;
        help.value_policy = klyspec::ValuePolicy::none;
        help.names = {"--help", "-h"};
        help.help = "Show command help";
        reg.register_argument(command, help);
        add_help_option_doc(command, HelpOptionDoc{{"--help", "-h"}, {}, "Show help for this command"});
    };

    add_command("version", "Print NuPERF version", "nuperf version [--help]", {});
    add_command("list-methods", "List registered methods", "nuperf list-methods [--help]", {"methods"});
    add_command("list-targets", "List registered targets", "nuperf list-targets [--help]", {"targets"});
    add_command(
        "build",
        "Build a table",
        "nuperf build [--input FILE | --from-json FILE | --from-yaml FILE | --from-sexpr FILE | --from-xml FILE] [options]",
        {},
        {"nuperf build --input keys.txt --method stdmeth --target stddef --output table.h",
         "nuperf build --from-json request.json --schema schema.json --output table.h",
         "cat keys.txt | nuperf build --method stdmeth --target stddef"});
    add_command(
        "init", "Initialize a NuPERF project directory", "nuperf init [DIR] [--add-build FILE] [--help]", {},
        {"nuperf init .", "nuperf init myproj --add-build CMakeLists.txt --add-build Makefile"});

    add_help_flag("version");
    add_help_flag("list-methods");
    add_help_flag("list-targets");
    add_help_flag("build");
    add_help_flag("init");

    klyspec::OptionSpec build_input;
    build_input.id = "input";
    build_input.names = {"--input", "-i"};
    reg.register_argument("build", build_input);
    add_help_option_doc("build", HelpOptionDoc{{"--input", "-i"}, "FILE", "Read newline-delimited keys from file (`-` for stdin)"});;

    klyspec::OptionSpec build_output;
    build_output.id = "output";
    build_output.names = {"--output", "-o"};
    build_output.default_value = "-";
    reg.register_argument("build", build_output);
    add_help_option_doc("build", HelpOptionDoc{{"--output", "-o"}, "FILE", "Write output to file (`-` for stdout)", false, false, std::string("-")});

    klyspec::OptionSpec build_method;
    build_method.id = "method";
    build_method.names = {"--method", "-m"};
    reg.register_argument("build", build_method);
    add_help_option_doc("build", HelpOptionDoc{{"--method", "-m"}, "NAME", "Select method plugin"});

    klyspec::OptionSpec build_target;
    build_target.id = "target";
    build_target.names = {"--target", "-t"};
    reg.register_argument("build", build_target);
    add_help_option_doc("build", HelpOptionDoc{{"--target", "-t"}, "NAME", "Select target plugin"});

    klyspec::OptionSpec build_schema;
    build_schema.id = "schema";
    build_schema.names = {"--schema"};
    reg.register_argument("build", build_schema);
    add_help_option_doc("build", HelpOptionDoc{{"--schema"}, "FILE", "Validate structured input against schema file"});

    klyspec::OptionSpec build_from_json;
    build_from_json.id = "from-json";
    build_from_json.names = {"--from-json"};
    reg.register_argument("build", build_from_json);
    add_help_option_doc("build", HelpOptionDoc{{"--from-json"}, "FILE", "Load structured build request from JSON"});

    klyspec::OptionSpec build_from_yaml;
    build_from_yaml.id = "from-yaml";
    build_from_yaml.names = {"--from-yaml"};
    reg.register_argument("build", build_from_yaml);
    add_help_option_doc("build", HelpOptionDoc{{"--from-yaml"}, "FILE", "Load structured build request from YAML"});

    klyspec::OptionSpec build_from_sexpr;
    build_from_sexpr.id = "from-sexpr";
    build_from_sexpr.names = {"--from-sexpr"};
    reg.register_argument("build", build_from_sexpr);
    add_help_option_doc("build", HelpOptionDoc{{"--from-sexpr"}, "FILE", "Load structured build request from S-expression"});

    klyspec::OptionSpec build_from_xml;
    build_from_xml.id = "from-xml";
    build_from_xml.names = {"--from-xml"};
    reg.register_argument("build", build_from_xml);
    add_help_option_doc("build", HelpOptionDoc{{"--from-xml"}, "FILE", "Load structured build request from XML"});

    klyspec::OptionSpec build_dump_schema;
    build_dump_schema.id = "dump-json-schema";
    build_dump_schema.names = {"--dump-json-schema"};
    reg.register_argument("build", build_dump_schema);
    add_help_option_doc("build", HelpOptionDoc{{"--dump-json-schema"}, "FILE", "Write build-request JSON schema to file"});

    klyspec::ArgumentSpec build_option;
    build_option.id = "option";
    build_option.kind = klyspec::ArgumentKind::repeatable;
    build_option.value_policy = klyspec::ValuePolicy::required;
    build_option.names = {"--option"};
    reg.register_argument("build", build_option);
    add_help_option_doc("build", HelpOptionDoc{{"--option"}, "KEY=VALUE", "Set table option (can be specified multiple times)", false, true});

    klyspec::PositionalSpec init_dir;
    init_dir.id = "dir";
    init_dir.index = 0;
    init_dir.required = false;
    reg.register_argument("init", init_dir);
    add_help_option_doc("init", HelpOptionDoc{{"DIR"}, {}, "Workspace directory (default: current directory)"});

    klyspec::ArgumentSpec init_add_build;
    init_add_build.id = "add-build";
    init_add_build.kind = klyspec::ArgumentKind::repeatable;
    init_add_build.value_policy = klyspec::ValuePolicy::required;
    init_add_build.names = {"--add-build"};
    reg.register_argument("init", init_add_build);
    add_help_option_doc("init", HelpOptionDoc{{"--add-build"}, "FILE", "Generate build scaffold file (`CMakeLists.txt` or `Makefile`)", false, true});

    HelpCommandDoc help_doc;
    help_doc.name = "help";
    help_doc.description = "Show global or command-specific help";
    help_doc.usage = "nuperf help [COMMAND] [usage|options|examples]";
    help_doc.examples = {"nuperf help", "nuperf help build", "nuperf help build options"};
    help_docs.push_back(std::move(help_doc));

    if (argc < 2) {
        print_global_help(help_docs);
        nuperf_shutdown();
        return 0;
    }

    const std::string cmd = argv[1];

    if (is_help_token(cmd)) {
        if (argc == 2) {
            print_global_help(help_docs);
            nuperf_shutdown();
            return 0;
        }
        const std::string command = argv[2];
        const auto *doc = find_help_command_doc(help_docs, command);
        if (!doc) {
            std::cerr << "unknown command for help: " << command << "\n";
            nuperf_shutdown();
            return 1;
        }
        const std::string section = (argc >= 4) ? std::string(argv[3]) : std::string{};
        if (!section.empty() && section != "usage" && section != "options" && section != "examples") {
            std::cerr << "unknown help section: " << section << "\n";
            nuperf_shutdown();
            return 1;
        }
        print_command_help(*doc, section);
        nuperf_shutdown();
        return 0;
    }

    if (cmd == "help") {
        if (argc == 2) {
            print_global_help(help_docs);
            nuperf_shutdown();
            return 0;
        }
        const std::string command = argv[2];
        const auto *doc = find_help_command_doc(help_docs, command);
        if (!doc) {
            std::cerr << "unknown command for help: " << command << "\n";
            nuperf_shutdown();
            return 1;
        }
        const std::string section = (argc >= 4) ? std::string(argv[3]) : std::string{};
        if (!section.empty() && section != "usage" && section != "options" && section != "examples") {
            std::cerr << "unknown help section: " << section << "\n";
            nuperf_shutdown();
            return 1;
        }
        print_command_help(*doc, section);
        nuperf_shutdown();
        return 0;
    }

    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) args.emplace_back(argv[i]);

    klyspec::KlyCLIService cli(reg);
    const auto pr = cli.parse(cmd, args);
    if (!pr.ok) {
        for (const auto &d : pr.diagnostics) std::cerr << d << "\n";
        if (const auto *doc = find_help_command_doc(help_docs, cmd)) {
            std::cerr << "Try: nuperf " << doc->name << " --help\n";
        }
        nuperf_shutdown();
        return 1;
    }

    const auto help_it = pr.values.find("help");
    if (help_it != pr.values.end()) {
        if (const auto *doc = find_help_command_doc(help_docs, cmd)) {
            print_command_help(*doc);
            nuperf_shutdown();
            return 0;
        }
    }

    int rc = 0;
    if (cmd == "version") {
        const auto v = nuperf_version();
        std::cout << v.major << "." << v.minor << "." << v.patch;
        if (v.suffix && *v.suffix) std::cout << "-" << v.suffix;
        std::cout << "\n";
    } else if (cmd == "list-methods" || cmd == "methods") {
        for (size_t i = 0; i < nuperf_method_count(); ++i) if (const char *n = nuperf_method_name(i)) std::cout << n << "\n";
    } else if (cmd == "list-targets" || cmd == "targets") {
        for (size_t i = 0; i < nuperf_target_count(); ++i) if (const char *n = nuperf_target_name(i)) std::cout << n << "\n";
    } else if (cmd == "build") {
        auto get1 = [&](const char *k) -> std::string {
            auto it = pr.values.find(k);
            return (it != pr.values.end() && !it->second.empty()) ? it->second.front() : std::string{};
        };
        nuperf::cli::InputFormat input_format = nuperf::cli::InputFormat::lines;
        std::string input_path = get1("input");
        const std::string from_json = get1("from-json");
        const std::string from_yaml = get1("from-yaml");
        const std::string from_sexpr = get1("from-sexpr");
        const std::string from_xml = get1("from-xml");
        std::size_t structured_count = 0;
        if (!from_json.empty()) { input_path = from_json; input_format = nuperf::cli::InputFormat::json; ++structured_count; }
        if (!from_yaml.empty()) { input_path = from_yaml; input_format = nuperf::cli::InputFormat::yaml; ++structured_count; }
        if (!from_sexpr.empty()) { input_path = from_sexpr; input_format = nuperf::cli::InputFormat::sexpr; ++structured_count; }
        if (!from_xml.empty()) { input_path = from_xml; input_format = nuperf::cli::InputFormat::xml; ++structured_count; }
        if (structured_count > 1) {
            std::cerr << "use only one of --from-json/--from-yaml/--from-sexpr/--from-xml\n";
            rc = 1;
        } else {
        const auto it = pr.values.find("option");
        const std::vector<std::string> opts = (it != pr.values.end()) ? it->second : std::vector<std::string>{};
        rc = cmd_build(input_path, get1("output"), get1("method"), get1("target"), opts, input_format,
                       get1("schema"), get1("dump-json-schema"));
        }
    } else if (cmd == "init") {
        const std::string dir = !pr.positionals.empty() ? pr.positionals.front() : std::string{};
        auto it = pr.values.find("add-build");
        const std::vector<std::string> builds = (it != pr.values.end()) ? it->second : std::vector<std::string>{};
        rc = cmd_init(dir, builds);
    } else {
        std::cerr << "unknown command: " << cmd << "\n";
        rc = 1;
    }

    nuperf_shutdown();
    return rc;
}
