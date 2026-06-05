#include <nuperf/nuperf-api.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    nuperf_status_t st = nuperf_init();
    nuperf_keyset_t *keys = NULL;
    nuperf_table_t *table = NULL;
    const uint8_t k1[] = {0x10, 0x20, 0x30};
    const uint8_t k2[] = {0x10, 0x20, 0x31};
    const uint8_t k3[] = {0x10, 0x20, 0x32};

    if (st == NUPERF_OK) st = nuperf_keyset_create(&keys);
    if (st == NUPERF_OK) st = nuperf_table_create(&table);
    if (st == NUPERF_OK) st = nuperf_keyset_add_binary(keys, k1, sizeof(k1));
    if (st == NUPERF_OK) st = nuperf_keyset_add_binary(keys, k2, sizeof(k2));
    if (st == NUPERF_OK) st = nuperf_keyset_add_binary(keys, k3, sizeof(k3));
    if (st == NUPERF_OK) st = nuperf_table_set_method(table, "stdmeth");
    if (st == NUPERF_OK) st = nuperf_table_set_target(table, "stddef");
    if (st == NUPERF_OK) st = nuperf_table_set_keyset(table, keys);
    if (st == NUPERF_OK) st = nuperf_table_build(table);

    printf("binary build: %s\n", nuperf_strerror(st));

    nuperf_table_destroy(table);
    nuperf_keyset_destroy(keys);
    nuperf_shutdown();
    return st == NUPERF_OK ? 0 : 1;
}
