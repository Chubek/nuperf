#include <catch2/catch_test_macros.hpp>

#include <nuperf/nuperf-api.h>
#include <nuperf/nuperf-method.h>
#include <nuperf/nuperf-target.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Fixture {
    Fixture() { (void)nuperf_init(); }
    ~Fixture() { nuperf_shutdown(); }
};

nuperf_status_t make_basic_built_table(nuperf_table_t **out_table, nuperf_keyset_t **out_keyset) {
    if (!out_table || !out_keyset) {
        return NUPERF_ERR_INVALID_ARGUMENT;
    }
    *out_table = nullptr;
    *out_keyset = nullptr;

    nuperf_status_t st = nuperf_keyset_create(out_keyset);
    if (st != NUPERF_OK) return st;

    st = nuperf_keyset_add_string(*out_keyset, "alpha");
    if (st != NUPERF_OK) return st;
    st = nuperf_keyset_add_string(*out_keyset, "beta");
    if (st != NUPERF_OK) return st;
    st = nuperf_keyset_add_string(*out_keyset, "gamma");
    if (st != NUPERF_OK) return st;

    st = nuperf_table_create(out_table);
    if (st != NUPERF_OK) return st;

    st = nuperf_table_set_keyset(*out_table, *out_keyset);
    if (st != NUPERF_OK) return st;

    st = nuperf_table_set_method(*out_table, "stdmeth");
    if (st != NUPERF_OK) return st;

    st = nuperf_table_set_target(*out_table, "stddef");
    if (st != NUPERF_OK) return st;

    return nuperf_table_build(*out_table);
}

size_t write_accumulate(const void *data, size_t size, void *user_data) {
    if (!user_data) {
        return 0;
    }
    auto *out = static_cast<std::string *>(user_data);
    if (!data && size != 0) {
        return 0;
    }
    out->append(static_cast<const char *>(data), size);
    return size;
}

size_t write_fail(const void *, size_t, void *) {
    return 0;
}

} // namespace

TEST_CASE("01: version reports non-null suffix", "[api][version]") {
    nuperf_version_t v = nuperf_version();
    CHECK(v.suffix != nullptr);
}

TEST_CASE("02: strerror for invalid argument", "[api][errors]") {
    CHECK(std::string(nuperf_strerror(NUPERF_ERR_INVALID_ARGUMENT)) == "invalid argument");
}

TEST_CASE("03: keyset create null output fails", "[api][keyset]") {
    CHECK(nuperf_keyset_create(nullptr) == NUPERF_ERR_INVALID_ARGUMENT);
}

TEST_CASE("04: keyset create and destroy", "[api][keyset]") {
    Fixture fix;
    nuperf_keyset_t *ks = nullptr;
    REQUIRE(nuperf_keyset_create(&ks) == NUPERF_OK);
    REQUIRE(ks != nullptr);
    nuperf_keyset_destroy(ks);
}

TEST_CASE("05: keyset add string null keyset fails", "[api][keyset]") {
    CHECK(nuperf_keyset_add_string(nullptr, "x") == NUPERF_ERR_INVALID_ARGUMENT);
}

TEST_CASE("06: keyset add string null key fails", "[api][keyset]") {
    Fixture fix;
    nuperf_keyset_t *ks = nullptr;
    REQUIRE(nuperf_keyset_create(&ks) == NUPERF_OK);
    CHECK(nuperf_keyset_add_string(ks, nullptr) == NUPERF_ERR_INVALID_ARGUMENT);
    nuperf_keyset_destroy(ks);
}

TEST_CASE("07: keyset type tracks string", "[api][keyset]") {
    Fixture fix;
    nuperf_keyset_t *ks = nullptr;
    REQUIRE(nuperf_keyset_create(&ks) == NUPERF_OK);
    REQUIRE(nuperf_keyset_add_string(ks, "abc") == NUPERF_OK);
    CHECK(nuperf_keyset_type(ks) == NUPERF_KEY_TYPE_STRING);
    CHECK(nuperf_keyset_size(ks) == 1);
    nuperf_keyset_destroy(ks);
}

TEST_CASE("08: mixed key types are rejected", "[api][keyset]") {
    Fixture fix;
    nuperf_keyset_t *ks = nullptr;
    REQUIRE(nuperf_keyset_create(&ks) == NUPERF_OK);
    REQUIRE(nuperf_keyset_add_string(ks, "abc") == NUPERF_OK);
    CHECK(nuperf_keyset_add_uint32(ks, 1) == NUPERF_ERR_INVALID_STATE);
    nuperf_keyset_destroy(ks);
}

TEST_CASE("09: keyset clear resets type and size", "[api][keyset]") {
    Fixture fix;
    nuperf_keyset_t *ks = nullptr;
    REQUIRE(nuperf_keyset_create(&ks) == NUPERF_OK);
    REQUIRE(nuperf_keyset_add_uint64(ks, 7) == NUPERF_OK);
    nuperf_keyset_clear(ks);
    CHECK(nuperf_keyset_type(ks) == NUPERF_KEY_TYPE_UNKNOWN);
    CHECK(nuperf_keyset_size(ks) == 0);
    nuperf_keyset_destroy(ks);
}

TEST_CASE("10: table create null output fails", "[api][table]") {
    CHECK(nuperf_table_create(nullptr) == NUPERF_ERR_INVALID_ARGUMENT);
}

TEST_CASE("11: table set keyset null args fail", "[api][table]") {
    Fixture fix;
    nuperf_table_t *table = nullptr;
    nuperf_keyset_t *ks = nullptr;
    REQUIRE(nuperf_table_create(&table) == NUPERF_OK);
    REQUIRE(nuperf_keyset_create(&ks) == NUPERF_OK);

    CHECK(nuperf_table_set_keyset(nullptr, ks) == NUPERF_ERR_INVALID_ARGUMENT);
    CHECK(nuperf_table_set_keyset(table, nullptr) == NUPERF_ERR_INVALID_ARGUMENT);

    nuperf_table_destroy(table);
    nuperf_keyset_destroy(ks);
}

TEST_CASE("12: table build without keyset returns invalid state", "[api][table]") {
    Fixture fix;
    nuperf_table_t *table = nullptr;
    REQUIRE(nuperf_table_create(&table) == NUPERF_OK);
    CHECK(nuperf_table_build(table) == NUPERF_ERR_INVALID_STATE);
    nuperf_table_destroy(table);
}

TEST_CASE("13: table build without method returns invalid state", "[api][table]") {
    Fixture fix;
    nuperf_table_t *table = nullptr;
    nuperf_keyset_t *ks = nullptr;
    REQUIRE(nuperf_table_create(&table) == NUPERF_OK);
    REQUIRE(nuperf_keyset_create(&ks) == NUPERF_OK);
    REQUIRE(nuperf_keyset_add_string(ks, "only") == NUPERF_OK);
    REQUIRE(nuperf_table_set_keyset(table, ks) == NUPERF_OK);
    CHECK(nuperf_table_build(table) == NUPERF_ERR_INVALID_STATE);
    nuperf_table_destroy(table);
    nuperf_keyset_destroy(ks);
}

TEST_CASE("14: method and target default plugins are discoverable", "[api][registry]") {
    Fixture fix;
    CHECK(nuperf_method_lookup("stdmeth") != nullptr);
    CHECK(nuperf_target_lookup("stddef") != nullptr);
}

TEST_CASE("15: table set unknown method fails", "[api][table]") {
    Fixture fix;
    nuperf_table_t *table = nullptr;
    REQUIRE(nuperf_table_create(&table) == NUPERF_OK);
    CHECK(nuperf_table_set_method(table, "does_not_exist") == NUPERF_ERR_NOT_FOUND);
    nuperf_table_destroy(table);
}

TEST_CASE("16: table set unknown target fails", "[api][table]") {
    Fixture fix;
    nuperf_table_t *table = nullptr;
    REQUIRE(nuperf_table_create(&table) == NUPERF_OK);
    CHECK(nuperf_table_set_target(table, "does_not_exist") == NUPERF_ERR_NOT_FOUND);
    nuperf_table_destroy(table);
}

TEST_CASE("17: successful build sets built state", "[api][build]") {
    Fixture fix;
    nuperf_table_t *table = nullptr;
    nuperf_keyset_t *ks = nullptr;
    REQUIRE(make_basic_built_table(&table, &ks) == NUPERF_OK);
    CHECK(nuperf_table_is_built(table) == NUPERF_TRUE);
    nuperf_table_destroy(table);
    nuperf_keyset_destroy(ks);
}

TEST_CASE("18: second build returns already built", "[api][build]") {
    Fixture fix;
    nuperf_table_t *table = nullptr;
    nuperf_keyset_t *ks = nullptr;
    REQUIRE(make_basic_built_table(&table, &ks) == NUPERF_OK);
    CHECK(nuperf_table_build(table) == NUPERF_ERR_ALREADY_BUILT);
    nuperf_table_destroy(table);
    nuperf_keyset_destroy(ks);
}

TEST_CASE("19: key count and key type after build", "[api][build]") {
    Fixture fix;
    nuperf_table_t *table = nullptr;
    nuperf_keyset_t *ks = nullptr;
    REQUIRE(make_basic_built_table(&table, &ks) == NUPERF_OK);
    CHECK(nuperf_table_key_count(table) == 3);
    CHECK(nuperf_table_key_type(table) == NUPERF_KEY_TYPE_STRING);
    nuperf_table_destroy(table);
    nuperf_keyset_destroy(ks);
}

TEST_CASE("20: stats before build returns not built", "[api][stats]") {
    Fixture fix;
    nuperf_table_t *table = nullptr;
    nuperf_build_stats_t stats{};
    REQUIRE(nuperf_table_create(&table) == NUPERF_OK);
    CHECK(nuperf_table_get_stats(table, &stats) == NUPERF_ERR_NOT_BUILT);
    nuperf_table_destroy(table);
}

TEST_CASE("21: get/set method option roundtrip", "[api][options]") {
    Fixture fix;
    nuperf_table_t *table = nullptr;
    REQUIRE(nuperf_table_create(&table) == NUPERF_OK);
    REQUIRE(nuperf_table_set_option(table, "method.seed", "123") == NUPERF_OK);

    size_t need = 0;
    REQUIRE(nuperf_table_get_option(table, "method.seed", nullptr, &need) == NUPERF_OK);
    std::vector<char> buffer(need, '\0');
    REQUIRE(nuperf_table_get_option(table, "method.seed", buffer.data(), &need) == NUPERF_OK);
    CHECK(std::string(buffer.data()) == "123");

    nuperf_table_destroy(table);
}

TEST_CASE("22: get option with small buffer reports needed size", "[api][options]") {
    Fixture fix;
    nuperf_table_t *table = nullptr;
    REQUIRE(nuperf_table_create(&table) == NUPERF_OK);
    REQUIRE(nuperf_table_set_option(table, "target.array_name", "mytable") == NUPERF_OK);

    char small[2] = {};
    size_t cap = sizeof(small);
    CHECK(nuperf_table_get_option(table, "target.array_name", small, &cap) == NUPERF_ERR_BUFFER_TOO_SMALL);
    CHECK(cap == std::strlen("mytable") + 1);

    nuperf_table_destroy(table);
}

TEST_CASE("23: set option with unknown prefix fails", "[api][options]") {
    Fixture fix;
    nuperf_table_t *table = nullptr;
    REQUIRE(nuperf_table_create(&table) == NUPERF_OK);
    CHECK(nuperf_table_set_option(table, "x.y", "1") == NUPERF_ERR_NOT_FOUND);
    nuperf_table_destroy(table);
}

TEST_CASE("24: get method name size query and fetch", "[api][table]") {
    Fixture fix;
    nuperf_table_t *table = nullptr;
    REQUIRE(nuperf_table_create(&table) == NUPERF_OK);
    REQUIRE(nuperf_table_set_method(table, "stdmeth") == NUPERF_OK);
    size_t need = 0;
    REQUIRE(nuperf_table_get_method(table, nullptr, &need) == NUPERF_OK);
    std::vector<char> name(need, '\0');
    REQUIRE(nuperf_table_get_method(table, name.data(), &need) == NUPERF_OK);
    CHECK(std::string(name.data()) == "stdmeth");
    nuperf_table_destroy(table);
}

TEST_CASE("25: get target name size query and fetch", "[api][table]") {
    Fixture fix;
    nuperf_table_t *table = nullptr;
    REQUIRE(nuperf_table_create(&table) == NUPERF_OK);
    REQUIRE(nuperf_table_set_target(table, "stddef") == NUPERF_OK);
    size_t need = 0;
    REQUIRE(nuperf_table_get_target(table, nullptr, &need) == NUPERF_OK);
    std::vector<char> name(need, '\0');
    REQUIRE(nuperf_table_get_target(table, name.data(), &need) == NUPERF_OK);
    CHECK(std::string(name.data()) == "stddef");
    nuperf_table_destroy(table);
}

TEST_CASE("26: emit buffer size query then emit data", "[api][emit]") {
    Fixture fix;
    nuperf_table_t *table = nullptr;
    nuperf_keyset_t *ks = nullptr;
    REQUIRE(make_basic_built_table(&table, &ks) == NUPERF_OK);

    size_t size = 0;
    REQUIRE(nuperf_table_emit_buffer(table, nullptr, &size) == NUPERF_OK);
    REQUIRE(size > 0);

    std::vector<char> out(size);
    REQUIRE(nuperf_table_emit_buffer(table, out.data(), &size) == NUPERF_OK);
    std::string text(out.data(), size);
    CHECK(text.find("static const uint32_t") != std::string::npos);

    nuperf_table_destroy(table);
    nuperf_keyset_destroy(ks);
}

TEST_CASE("27: emit writer accumulates output", "[api][emit]") {
    Fixture fix;
    nuperf_table_t *table = nullptr;
    nuperf_keyset_t *ks = nullptr;
    REQUIRE(make_basic_built_table(&table, &ks) == NUPERF_OK);

    std::string out;
    REQUIRE(nuperf_table_emit_writer(table, write_accumulate, &out) == NUPERF_OK);
    CHECK(out.find("Generated by NuPERF") != std::string::npos);

    nuperf_table_destroy(table);
    nuperf_keyset_destroy(ks);
}

TEST_CASE("28: emit writer short write fails with io", "[api][emit]") {
    Fixture fix;
    nuperf_table_t *table = nullptr;
    nuperf_keyset_t *ks = nullptr;
    REQUIRE(make_basic_built_table(&table, &ks) == NUPERF_OK);
    CHECK(nuperf_table_emit_writer(table, write_fail, nullptr) == NUPERF_ERR_IO);
    nuperf_table_destroy(table);
    nuperf_keyset_destroy(ks);
}

TEST_CASE("29: emit file writes output file", "[api][emit]") {
    Fixture fix;
    nuperf_table_t *table = nullptr;
    nuperf_keyset_t *ks = nullptr;
    REQUIRE(make_basic_built_table(&table, &ks) == NUPERF_OK);

    const char *path = "test-output-nuperf.h";
    REQUIRE(nuperf_table_emit_file(table, path) == NUPERF_OK);

    FILE *f = std::fopen(path, "rb");
    REQUIRE(f != nullptr);
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fclose(f);
    CHECK(sz > 0);

    nuperf_table_destroy(table);
    nuperf_keyset_destroy(ks);
}

TEST_CASE("30: registry indexed access is consistent", "[api][registry]") {
    Fixture fix;
    const size_t mc = nuperf_method_count();
    const size_t tc = nuperf_target_count();
    REQUIRE(mc > 0);
    REQUIRE(tc > 0);
    for (size_t i = 0; i < mc; ++i) {
        REQUIRE(nuperf_method_name(i) != nullptr);
        REQUIRE(nuperf_method_at(i) != nullptr);
    }
    for (size_t i = 0; i < tc; ++i) {
        REQUIRE(nuperf_target_name(i) != nullptr);
        REQUIRE(nuperf_target_at(i) != nullptr);
    }
    CHECK(nuperf_method_name(mc) == nullptr);
    CHECK(nuperf_target_name(tc) == nullptr);
}
