#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace {

struct CommandResult {
    int exit_code{1};
    std::string output;
};

std::string cli_path() {
    if (const char *env = std::getenv("NUPERF_CLI_PATH")) {
        if (*env) return std::string(env);
    }
    if (std::filesystem::exists("./nuperf")) return "./nuperf";
    if (std::filesystem::exists("build/nuperf")) return "build/nuperf";
    return "nuperf";
}

CommandResult run_command(const std::string &cmd) {
    CommandResult result{};
    std::array<char, 512> buffer{};
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        result.output = "popen failed";
        return result;
    }

    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result.output += buffer.data();
    }

    const int status = pclose(pipe);
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else {
        result.exit_code = 1;
    }
    return result;
}

std::filesystem::path make_temp_input_file() {
    const auto base = std::filesystem::temp_directory_path() /
                      ("nuperf-cli-test-" + std::to_string(::getpid()) + ".txt");
    std::ofstream out(base);
    out << "foo\nbar\nbaz\n";
    return base;
}

} // namespace

TEST_CASE("CLI 01: version command exits successfully", "[cli][smoke]") {
    const auto result = run_command(cli_path() + " version 2>&1");
    REQUIRE(result.exit_code == 0);
    CHECK(result.output.find("0.1.0") != std::string::npos);
}

TEST_CASE("CLI 02: list-methods includes stdmeth", "[cli][smoke]") {
    const auto result = run_command(cli_path() + " registry methods list 2>&1");
    REQUIRE(result.exit_code == 0);
    CHECK(result.output.find("stdmeth") != std::string::npos);
}

TEST_CASE("CLI 03: list-targets includes stddef", "[cli][smoke]") {
    const auto result = run_command(cli_path() + " registry targets list 2>&1");
    REQUIRE(result.exit_code == 0);
    CHECK(result.output.find("stddef") != std::string::npos);
}

TEST_CASE("CLI 04: build with input uses default method/target", "[cli][smoke]") {
    const auto input = make_temp_input_file();
    const auto cmd = cli_path() + " build run --input " + input.string() + " 2>&1";
    const auto result = run_command(cmd);
    std::filesystem::remove(input);

    REQUIRE(result.exit_code == 0);
    CHECK(result.output.find("#ifndef NUPERF_TABLE_H") != std::string::npos);
    CHECK(result.output.find("static const uint32_t nuperf_table[]") != std::string::npos);
}

TEST_CASE("CLI 05: build with unknown method fails", "[cli][smoke]") {
    const auto input = make_temp_input_file();
    const auto cmd = cli_path() + " build run --input " + input.string() + " --method nope 2>&1";
    const auto result = run_command(cmd);
    std::filesystem::remove(input);

    REQUIRE(result.exit_code != 0);
    CHECK(result.output.find("set_method: not found") != std::string::npos);
}

TEST_CASE("CLI 06: build with malformed option fails", "[cli][smoke]") {
    const auto input = make_temp_input_file();
    const auto cmd = cli_path() + " build run --input " + input.string() + " --option badopt 2>&1";
    const auto result = run_command(cmd);
    std::filesystem::remove(input);

    REQUIRE(result.exit_code != 0);
    CHECK(result.output.find("invalid --option") != std::string::npos);
}

TEST_CASE("CLI 07: global help shows groups", "[cli][smoke]") {
    const auto result = run_command(cli_path() + " help 2>&1");
    REQUIRE(result.exit_code == 0);
    CHECK(result.output.find("registry") != std::string::npos);
    CHECK(result.output.find("build") != std::string::npos);
    CHECK(result.output.find("project") != std::string::npos);
}

TEST_CASE("CLI 08: group help works", "[cli][smoke]") {
    const auto result = run_command(cli_path() + " help registry methods 2>&1");
    REQUIRE(result.exit_code == 0);
    CHECK(result.output.find("registry methods") != std::string::npos);
    CHECK(result.output.find("list") != std::string::npos);
}

TEST_CASE("CLI 09: command help works", "[cli][smoke]") {
    const auto result = run_command(cli_path() + " help build run 2>&1");
    REQUIRE(result.exit_code == 0);
    CHECK(result.output.find("nuperf build run") != std::string::npos);
    CHECK(result.output.find("--method") != std::string::npos);
}
