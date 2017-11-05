#include <stdint.h>
#include <string.h>
#include "auxiliary.h"
#include "config.h"
#include "error.h"
#include "sigil.h"

sigil_err_t process_header(sigil_t *sgl)
{
    // function parameter checks
    if (sgl == NULL || sgl->file == NULL) {
        return (sigil_err_t)ERR_PARAM;
    }

    const char expected[] = {'%', 'P', 'D', 'F', '-'};
    size_t offset = 0;
    int found = 0,
        c;

    while ((c = fgetc(sgl->file)) != EOF && found < 8 &&
            offset - found <= HEADER_SEARCH_OFFSET    )
    {
        // count offset from start to '%' char
        // PDF header size is subtracted later
        offset++;

        if (found < 5) {
            if (c == (int)expected[found]) {
                found++;
            } else if (c == (int)expected[0]) {
                found = 1;
            } else {
                found = 0;
            }
        } else if (found == 5) {
            if (is_digit(c)) {
                sgl->pdf_x = c - '0';
                found++;
            } else if (c == (int)expected[0]) {
                found = 1;
            } else {
                found = 0;
            }
        } else if (found == 6) {
            if (c == (int)'.') {
                found++;
            } else if (c == (int)expected[0]) {
                found = 1;
            } else {
                found = 0;
            }
        } else if (found == 7) {
            if (is_digit(c)) {
                sgl->pdf_y = c - '0';
                found++;
            } else if (c == (int)expected[0]) {
                found = 1;
            } else {
                found = 0;
            }
        }
    }

    if (found != 8) {
        return (sigil_err_t)ERR_PDF_CONT;
    }

    // offset counted with header -> subtract header size
    sgl->pdf_start_offset = offset - 8;

    return (sigil_err_t)ERR_NO;
}

int sigil_header_self_test(int quiet)
{
    if (!quiet)
        printf("\n + Testing module: header\n");

    // prepare
    sigil_t *sgl = NULL;

    if (sigil_init(&sgl) != ERR_NO)
        goto failed;

    // TEST: correct_1
    if (!quiet)
        printf("    - %-30s", "correct_1");

    char *correct_1 = "%PDF-1.1\n"                \
                      "abcdefghijklmnopqrstuvwxyz";
    sgl->file = fmemopen(correct_1,
                         (strlen(correct_1) + 1) * sizeof(char),
                         "r");
    if (sgl->file == NULL)
        goto failed;

    if (process_header(sgl) != ERR_NO ||
        sgl->pdf_x != 1               ||
        sgl->pdf_y != 1               ||
        sgl->pdf_start_offset != 0     )
    {
        if (!quiet)
            printf("FAILED\n");

        goto failed;
    }

    if (!quiet)
        printf("OK\n");

    fclose(sgl->file);
    sgl->file = NULL;

    // TEST: correct_2
    if (!quiet)
        printf("    - %-30s", "correct_2");

    char *correct_2 = "\x1a\x5e\x93\x7e\x6f\x3c\x6a\x71\xbf\xda\x54\x91\xe5"   \
                      "\x86\x08\x84\xaf\x8e\x89\x44\xab\xc4\x58\x0c\xb9\x31"   \
                      "\xd3\x8c\x0f\xc0\x43\x1a\xa5\x07\x4f\xe2\x98\xb3\xd8"   \
                      "\x53\x4b\x5d\x4b\xd6\x48\x26\x98\x09\xde\x0d"           \
                      "%PDF-1.2"                                               \
                      "\x55\xa1\x77\xd3\x47\xab\xc6\x87\xf3\xbc\x2d\x8a\x9f"   \
                      "\x0e\x47\xbb\x74\xd2\x71\x28\x94\x53\x92\xae\x2b\x17"   \
                      "\xd0\x6a\x9c\x13\x84\xc1\x07\x44\xc0\x81\xb8\xd6\x9c"   \
                      "\x31\x08\x13\xd4\xc2\xd6\x2d\xaf\xfb\xea\x6f";
    sgl->file = fmemopen(correct_2,
                         (strlen(correct_2) + 1) * sizeof(char),
                         "r");
    if (sgl->file == NULL)
        goto failed;

    if (process_header(sgl) != ERR_NO ||
        sgl->pdf_x != 1               ||
        sgl->pdf_y != 2               ||
        sgl->pdf_start_offset != 50    )
    {
        if (!quiet)
            printf("FAILED\n");

        goto failed;
    }

    if (!quiet)
        printf("OK\n");

    fclose(sgl->file);
    sgl->file = NULL;

    // TODO add at least one wrong

    // all tests done
    if (!quiet) {
        printf("   PASSED\n");
        fflush(stdout);
    }

    return 0;

failed:
    if (!quiet) {
        printf("   FAILED\n");
        fflush(stdout);
    }

    return 1;
}
