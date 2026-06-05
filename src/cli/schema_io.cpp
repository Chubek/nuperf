#include "schema_io.hpp"

#include <SerdeTk.hpp>

#include <sstream>

namespace nuperf::cli {
namespace {

using serdetk::Document;
using serdetk::Object;
using serdetk::Value;

Document load_document(const std::filesystem::path &path, InputFormat format) {
    switch (format) {
        case InputFormat::json: return serdetk::json::from_file(path);
        case InputFormat::yaml: return serdetk::yaml::from_file(path);
        case InputFormat::sexpr: return serdetk::sexpr::from_file(path);
        case InputFormat::xml: return serdetk::xml::from_file(path);
        case InputFormat::lines: break;
    }
    return {};
}

bool validate_document(
    const Document &document,
    const std::filesystem::path &schema_path,
    std::string &out_error) {
    if (schema_path.empty()) {
        return true;
    }
    try {
        auto validator = serdetk::json::Validator::from_schema_file(schema_path);
        const auto report = validator->validate(document);
        if (report.success) {
            return true;
        }
        std::ostringstream oss;
        report.print(oss);
        out_error = oss.str();
        return false;
    } catch (const std::exception &e) {
        out_error = e.what();
        return false;
    } catch (...) {
        out_error = "schema validation failed";
        return false;
    }
}

bool collect_keys_from_value(
    const Value &value,
    std::vector<std::string> &out_keys,
    std::string &out_error) {
    if (!value.is_array()) {
        out_error = "keys must be an array";
        return false;
    }
    for (const auto &item : value.as_array().items) {
        if (!item.is_string()) {
            out_error = "all keys must be strings";
            return false;
        }
        out_keys.push_back(item.as_string());
    }
    return true;
}

bool extract_request_from_document(
    const Document &document,
    BuildRequest &out_request,
    std::string &out_error) {
    if (document.root.is_array()) {
        return collect_keys_from_value(document.root, out_request.keys, out_error);
    }

    if (!document.root.is_object()) {
        out_error = "document root must be an object or array";
        return false;
    }

    const Object &root = document.root.as_object();
    if (root.contains("keys")) {
        if (!collect_keys_from_value(root.at("keys"), out_request.keys, out_error)) {
            return false;
        }
    } else if (root.contains("dataset")) {
        const Value &dataset = root.at("dataset");
        if (!dataset.is_object() || !dataset.as_object().contains("keys")) {
            out_error = "dataset.keys is required";
            return false;
        }
        if (!collect_keys_from_value(dataset.as_object().at("keys"), out_request.keys, out_error)) {
            return false;
        }
    } else {
        out_error = "missing keys array";
        return false;
    }

    if (root.contains("method") && root.at("method").is_string()) {
        out_request.method = root.at("method").as_string();
    }
    if (root.contains("target") && root.at("target").is_string()) {
        out_request.target = root.at("target").as_string();
    }
    if (root.contains("options")) {
        const Value &options = root.at("options");
        if (!options.is_object()) {
            out_error = "options must be an object";
            return false;
        }
        for (const auto &entry : options.as_object().fields) {
            const Value &option_value = entry.second;
            if (option_value.is_string()) {
                out_request.options.push_back(entry.first + "=" + option_value.as_string());
            } else if (option_value.is_bool()) {
                out_request.options.push_back(entry.first + "=" + std::string(std::get<bool>(option_value.data) ? "true" : "false"));
            } else if (option_value.is_int()) {
                out_request.options.push_back(entry.first + "=" + std::to_string(std::get<std::int64_t>(option_value.data)));
            } else if (option_value.is_uint()) {
                out_request.options.push_back(entry.first + "=" + std::to_string(std::get<std::uint64_t>(option_value.data)));
            } else if (option_value.is_double()) {
                out_request.options.push_back(entry.first + "=" + std::to_string(std::get<double>(option_value.data)));
            } else {
                out_error = "option values must be scalar";
                return false;
            }
        }
    }

    return true;
}

} // namespace

bool load_build_request(
    const std::filesystem::path &path,
    InputFormat format,
    const std::filesystem::path &schema_path,
    BuildRequest &out_request,
    std::string &out_error) {
    try {
        if (format == InputFormat::lines) {
            return false;
        }
        const Document document = load_document(path, format);
        if (!validate_document(document, schema_path, out_error)) {
            return false;
        }
        return extract_request_from_document(document, out_request, out_error);
    } catch (const std::exception &e) {
        out_error = e.what();
        return false;
    } catch (...) {
        out_error = "failed to load structured dataset";
        return false;
    }
}

std::string canonical_dataset_schema_json() {
    return R"JSON({
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "NuPERF Dataset",
  "description": "Structured input document for NuPERF CLI imports.",
  "oneOf": [
    {
      "type": "array",
      "items": { "type": "string" }
    },
    {
      "type": "object",
      "required": ["keys"],
      "additionalProperties": false,
      "properties": {
        "keys": {
          "type": "array",
          "items": { "type": "string" }
        },
        "method": { "type": "string" },
        "target": { "type": "string" },
        "options": {
          "type": "object",
          "additionalProperties": {
            "type": ["string", "boolean", "integer", "number"]
          }
        }
      }
    },
    {
      "type": "object",
      "required": ["dataset"],
      "additionalProperties": false,
      "properties": {
        "dataset": {
          "type": "object",
          "required": ["keys"],
          "additionalProperties": false,
          "properties": {
            "keys": {
              "type": "array",
              "items": { "type": "string" }
            }
          }
        },
        "method": { "type": "string" },
        "target": { "type": "string" },
        "options": {
          "type": "object",
          "additionalProperties": {
            "type": ["string", "boolean", "integer", "number"]
          }
        }
      }
    }
  ]
})JSON";
}

bool dump_dataset_schema(
    const std::filesystem::path &path,
    std::string &out_error) {
    try {
        serdetk::json::dump_to_file(
            serdetk::json::from_string(canonical_dataset_schema_json()),
            path);
        return true;
    } catch (const std::exception &e) {
        out_error = e.what();
        return false;
    } catch (...) {
        out_error = "failed to write schema";
        return false;
    }
}

} // namespace nuperf::cli
