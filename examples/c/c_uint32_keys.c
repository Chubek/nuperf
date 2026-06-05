#include <nuperf/nuperf-api.h>
#include <stdio.h>

int main(void) {
    nuperf_status_t st = nuperf_init();
    nuperf_keyset_t *keys = NULL;
    nuperf_table_t *table = NULL;

    if (st == NUPERF_OK) st = nuperf_keyset_create(&keys);
    if (st == NUPERF_OK) st = nuperf_table_create(&table);
    if (st == NUPERF_OK) st = nuperf_keyset_add_uint32(keys, 10);
    if (st == NUPERF_OK) st = nuperf_keyset_add_uint32(keys, 42);
    if (st == NUPERF_OK) st = nuperf_keyset_add_uint32(keys, 77);
    if (st == NUPERF_OK) st = nuperf_keyset_add_uint32(keys, 99);
    if (st == NUPERF_OK) st = nuperf_table_set_method(table, "stdmeth");
    if (st == NUPERF_OK) st = nuperf_table_set_target(table, "stddef");
    if (st == NUPERF_OK) st = nuperf_table_set_keyset(table, keys);
    if (st == NUPERF_OK) st = nuperf_table_build(table);

    printf("u32 build: %s\n", nuperf_strerror(st));

    nuperf_table_destroy(table);
    nuperf_keyset_destroy(keys);
    nuperf_shutdown();
    return st == NUPERF_OK ? 0 : 1;
}
