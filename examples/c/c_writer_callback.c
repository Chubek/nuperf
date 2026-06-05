#include <nuperf/nuperf-api.h>
#include <stdio.h>

static size_t writer(const void *data, size_t size, void *user_data) {
    FILE *out = (FILE *)user_data;
    return fwrite(data, 1, size, out);
}

int main(void) {
    nuperf_status_t st = nuperf_init();
    nuperf_keyset_t *keys = NULL;
    nuperf_table_t *table = NULL;

    if (st == NUPERF_OK) st = nuperf_keyset_create(&keys);
    if (st == NUPERF_OK) st = nuperf_table_create(&table);
    if (st == NUPERF_OK) st = nuperf_keyset_add_string(keys, "one");
    if (st == NUPERF_OK) st = nuperf_keyset_add_string(keys, "two");
    if (st == NUPERF_OK) st = nuperf_keyset_add_string(keys, "three");
    if (st == NUPERF_OK) st = nuperf_table_set_method(table, "stdmeth");
    if (st == NUPERF_OK) st = nuperf_table_set_target(table, "stddef");
    if (st == NUPERF_OK) st = nuperf_table_set_keyset(table, keys);
    if (st == NUPERF_OK) st = nuperf_table_build(table);
    if (st == NUPERF_OK) st = nuperf_table_emit_writer(table, writer, stdout);

    nuperf_table_destroy(table);
    nuperf_keyset_destroy(keys);
    nuperf_shutdown();
    return st == NUPERF_OK ? 0 : 1;
}
