#include <nuperf/nuperf-api.h>
#include <stdio.h>

int main(void) {
    nuperf_status_t st = nuperf_init();
    nuperf_keyset_t *keys = NULL;
    nuperf_table_t *table = NULL;

    if (st == NUPERF_OK) st = nuperf_keyset_create(&keys);
    if (st == NUPERF_OK) st = nuperf_table_create(&table);
    if (st == NUPERF_OK) st = nuperf_keyset_add_string(keys, "alpha");
    if (st == NUPERF_OK) st = nuperf_keyset_add_string(keys, "beta");
    if (st == NUPERF_OK) st = nuperf_keyset_add_string(keys, "gamma");
    if (st == NUPERF_OK) st = nuperf_table_set_method(table, "stdmeth");
    if (st == NUPERF_OK) st = nuperf_table_set_target(table, "stddef");
    if (st == NUPERF_OK) st = nuperf_table_set_keyset(table, keys);
    if (st == NUPERF_OK) st = nuperf_table_build(table);

    printf("status=%s keys=%zu\n", nuperf_strerror(st), nuperf_table_key_count(table));

    nuperf_table_destroy(table);
    nuperf_keyset_destroy(keys);
    nuperf_shutdown();
    return st == NUPERF_OK ? 0 : 1;
}
