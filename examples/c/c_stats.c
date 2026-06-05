#include <nuperf/nuperf-api.h>
#include <stdio.h>

int main(void) {
    nuperf_status_t st = nuperf_init();
    nuperf_keyset_t *keys = NULL;
    nuperf_table_t *table = NULL;

    if (st == NUPERF_OK) st = nuperf_keyset_create(&keys);
    if (st == NUPERF_OK) st = nuperf_table_create(&table);
    if (st == NUPERF_OK) st = nuperf_keyset_add_string(keys, "north");
    if (st == NUPERF_OK) st = nuperf_keyset_add_string(keys, "south");
    if (st == NUPERF_OK) st = nuperf_keyset_add_string(keys, "east");
    if (st == NUPERF_OK) st = nuperf_keyset_add_string(keys, "west");
    if (st == NUPERF_OK) st = nuperf_table_set_method(table, "stdmeth");
    if (st == NUPERF_OK) st = nuperf_table_set_target(table, "stddef");
    if (st == NUPERF_OK) st = nuperf_table_set_keyset(table, keys);
    if (st == NUPERF_OK) st = nuperf_table_build(table);

    nuperf_build_stats_t stats;
    if (st == NUPERF_OK) st = nuperf_table_get_stats(table, &stats);

    if (st == NUPERF_OK) {
        printf("keys=%zu bytes=%zu build_us=%llu bits_per_key=%.6f\n",
               stats.key_count,
               stats.table_size_bytes,
               (unsigned long long)stats.build_time_us,
               stats.bits_per_key);
    }

    nuperf_table_destroy(table);
    nuperf_keyset_destroy(keys);
    nuperf_shutdown();
    return st == NUPERF_OK ? 0 : 1;
}
