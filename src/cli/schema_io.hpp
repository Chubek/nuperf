#ifndef NUPERF_CLI_SCHEMA_IO_HPP
#define NUPERF_CLI_SCHEMA_IO_HPP

#include <nuperf/nuperf-api.h>

#include <filesystem>
#include <string>
#include <vector>

namespace nuperf::cli {

enum class InputFormat {
    lines,
    json,
    yaml,
    sexpr,
    xml
};

struct BuildRequest {
    std::vector<std::string> keys;
    std::string method;
    std::string target;
    std::vector<std::string> options;
};

bool load_build_request(
    const std::filesystem::path &path,
    InputFormat format,
    const std::filesystem::path &schema_path,
    BuildRequest &out_request,
    std::string &out_error);

std::string canonical_dataset_schema_json();

bool dump_dataset_schema(
    const std::filesystem::path &path,
    std::string &out_error);

} // namespace nuperf::cli

#endif
