#include <nuperf/nuperf-api.h>
#include <stdio.h>

int main(void) {
    nuperf_status_t st = nuperf_init();
    if (st != NUPERF_OK) {
        fprintf(stderr, "init failed: %s\n", nuperf_strerror(st));
        return 1;
    }

    size_t methods = nuperf_method_count();
    size_t targets = nuperf_target_count();

    printf("methods (%zu):\n", methods);
    for (size_t i = 0; i < methods; ++i) {
        printf("  - %s\n", nuperf_method_name(i));
    }

    printf("targets (%zu):\n", targets);
    for (size_t i = 0; i < targets; ++i) {
        printf("  - %s\n", nuperf_target_name(i));
    }

    nuperf_shutdown();
    return 0;
}
