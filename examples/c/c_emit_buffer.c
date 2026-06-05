#include <nuperf/nuperf-api.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    nuperf_status_t st = nuperf_init();
    nuperf_keyset_t *keys = NULL;
    nuperf_table_t *table = NULL;
    size_t out_size = 0;

    if (st == NUPERF_OK) st = nuperf_keyset_create(&keys);
    if (st == NUPERF_OK) st = nuperf_table_create(&table);
    if (st == NUPERF_OK) st = nuperf_keyset_add_string(keys, "dog");
    if (st == NUPERF_OK) st = nuperf_keyset_add_string(keys, "cat");
    if (st == NUPERF_OK) st = nuperf_keyset_add_string(keys, "fox");
    if (st == NUPERF_OK) st = nuperf_table_set_method(table, "stdmeth");
    if (st == NUPERF_OK) st = nuperf_table_set_target(table, "stddef");
    if (st == NUPERF_OK) st = nuperf_table_set_keyset(table, keys);
    if (st == NUPERF_OK) st = nuperf_table_build(table);
    if (st == NUPERF_OK) st = nuperf_table_emit_buffer(table, NULL, &out_size);

    char *buffer = NULL;
    if (st == NUPERF_OK) {
        buffer = (char *)malloc(out_size + 1);
        if (!buffer) st = NUPERF_ERR_OUT_OF_MEMORY;
    }
    if (st == NUPERF_OK) st = nuperf_table_emit_buffer(table, buffer, &out_size);
    if (st == NUPERF_OK && buffer) {
        buffer[out_size] = '\0';
        puts(buffer);
    }

    free(buffer);
    nuperf_table_destroy(table);
    nuperf_keyset_destroy(keys);
    nuperf_shutdown();
    return st == NUPERF_OK ? 0 : 1;
}
