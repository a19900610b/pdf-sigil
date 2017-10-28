#include <string.h>
#include "error.h"
#include "sigil.h"

int main(int argc, char **argv)
{
    int quiet  = 0,
        failed = 0;

    if (argc == 2 && (strcmp(argv[1], "-q" ) == 0  ||
                      strcmp(argv[1], "--quiet" ) == 0))
    {
        quiet = 1;
    }

    if (!quiet)
        printf("\n STARTING TEST PROCEDURE\n");

    if (sigil_error_self_test(quiet) != 0) {
        failed++;
    }
    if (sigil_sigil_self_test(quiet) != 0) {
        failed++;
    }

    if (!quiet)
        printf("\n TOTAL FAILED: %d\n", failed);

    return (failed != 0);
}
