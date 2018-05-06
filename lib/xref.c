#include <stdlib.h>
#include <string.h>
#include <types.h>
#include "auxiliary.h"
#include "config.h"
#include "constants.h"
#include "sigil.h"
#include "xref.h"

// Determine whether this file is using Cross-reference table or stream
static sigil_err_t determine_xref_type(sigil_t *sgl)
{
    sigil_err_t err;
    char c;

    if (sgl == NULL)
        return ERR_PARAMETER;

    if ((err = pdf_peek_char(sgl, &c)) != ERR_NONE)
        return err;

    if (c == 'x') {
        sgl->xref_type = XREF_TYPE_TABLE;
    } else if (is_digit(c)) {
        sgl->xref_type = XREF_TYPE_STREAM;
    } else {
        return ERR_PDF_CONTENT;
    }

    return ERR_NONE;
}

static sigil_err_t
add_xref_entry(xref_t *xref, size_t obj, size_t offset, size_t generation)
{
    int resize_factor = 1;

    if (xref == NULL)
        return ERR_PARAMETER;

    // resize if needed
    while (obj > resize_factor * xref->capacity - 1) {
        resize_factor *= 2;
    }

    if (resize_factor != 1) {
        xref->entry = realloc(xref->entry, sizeof(xref_entry_t *) * xref->capacity * resize_factor);
        if (xref->entry == NULL)
            return ERR_ALLOCATION;
        sigil_zeroize(xref->entry + xref->capacity, sizeof(xref_entry_t *) * (xref->capacity * (resize_factor - 1)));
        xref->capacity *= resize_factor;
    }

    xref_entry_t **xref_entry = &(xref->entry[obj]);

    while (*xref_entry != NULL) {
        if ((*xref_entry)->generation_num == generation)
            return ERR_NONE;

        xref_entry = &(*xref_entry)->next;
    }

    *xref_entry = malloc(sizeof(xref_entry_t));
    if (*xref_entry == NULL)
        return ERR_ALLOCATION;
    sigil_zeroize(*xref_entry, sizeof(xref_entry_t));

    (*xref_entry)->byte_offset = offset;
    (*xref_entry)->generation_num = generation;

    return ERR_NONE;
}

static void free_xref_entry(xref_entry_t *entry)
{
    if (entry != NULL) {
        free_xref_entry(entry->next);
        sigil_zeroize(entry, sizeof(*entry));
        free(entry);
    }
}

xref_t *xref_init(void)
{
    xref_t *xref = malloc(sizeof(xref_t));
    if (xref == NULL)
        return NULL;
    sigil_zeroize(xref, sizeof(*xref));

    xref->entry = malloc(sizeof(xref_entry_t *) * XREF_PREALLOCATION);
    if (xref->entry == NULL) {
        free(xref);
        return NULL;
    }
    sigil_zeroize(xref->entry, sizeof(*(xref->entry)) * XREF_PREALLOCATION);
    xref->capacity = XREF_PREALLOCATION;
    xref->size_from_trailer = 0;
    xref->prev_section = 0;

    return xref;
}

void xref_free(xref_t *xref)
{
    if (xref == NULL)
        return;

    if (xref->entry != NULL) {
        for (size_t i = 0; i < xref->capacity; i++) {
            free_xref_entry(xref->entry[i]);
        }
        free(xref->entry);
    }

    sigil_zeroize(xref, sizeof(*xref));
    free(xref);
}

sigil_err_t read_startxref(sigil_t *sgl)
{
    sigil_err_t err;
    size_t offset;
    size_t read_size;
    char tmp[10];

    if (sgl == NULL)
        return ERR_PARAMETER;

    for (offset = 9; offset < XREF_SEARCH_OFFSET; offset++) {
        if ((err = pdf_move_pos_abs(sgl, sgl->pdf_data.size - offset)) != ERR_NONE)
            return err;
        
        err = pdf_read(sgl, 9, tmp, &read_size);
        if (err != ERR_NONE)
            return err;
        if (read_size != 9)
            return ERR_PDF_CONTENT;

        if (strncmp(tmp, "startxref", 9) == 0) {
            if ((err = parse_number(sgl, &(sgl->offset_startxref))) != ERR_NONE)
                return err;
            if (sgl->offset_startxref == 0)
                return ERR_PDF_CONTENT;

            return ERR_NONE;
        }
    }

    return ERR_PDF_CONTENT;
}

/** @brief Reads all the entries from the cross-reference table to the context
 *
 * @param sgl context
 * @return ERR_NONE if success
 */
static sigil_err_t read_xref_table(sigil_t *sgl)
{
    size_t section_start = 0,
           section_cnt = 0,
           obj_offset,
           obj_generation;
    int xref_end = 0;
    sigil_err_t err;

    if (sgl == NULL)
        return ERR_PARAMETER;

    if (sgl->xref == NULL) {
        sgl->xref = xref_init();
        if (sgl->xref == NULL)
            return ERR_ALLOCATION;
    }

    if ((err = skip_word(sgl, "xref")) != ERR_NONE)
        return err;

    while (!xref_end) { // for all xref sections
        while (1) {
            // read 2 numbers:
            //     - first object in subsection
            //     - number of entries in subsection
            if (parse_number(sgl, &section_start) != ERR_NONE) {
                xref_end = 1;
                break;
            }
            if (parse_number(sgl, &section_cnt) != ERR_NONE)
                return 1;
            if (section_start < 0 || section_cnt < 1)
                return 1;

            // for all entries in one section
            for (size_t section_offset = 0; section_offset < section_cnt; section_offset++) {
                err = parse_number(sgl, &obj_offset);
                if (err != ERR_NONE)
                    return err;

                err = parse_number(sgl, &obj_generation);
                if (err != ERR_NONE)
                    return err;

                if (skip_word(sgl, "f") == ERR_NONE)
                    continue;
                err = skip_word(sgl, "n");
                if (err != ERR_NONE)
                    return err;

                size_t obj_num = section_start + section_offset;

                err = add_xref_entry(sgl->xref, obj_num, obj_offset, obj_generation);
                if (err != ERR_NONE)
                    return err;
            }
        }
    }

    return ERR_NONE;
}

sigil_err_t process_xref(sigil_t *sgl)
{
    sigil_err_t err;

    if (sgl == NULL)
        return ERR_PARAMETER;

    err = determine_xref_type(sgl);
    if (err != ERR_NONE)
        return err;

    switch (sgl->xref_type) {
        case XREF_TYPE_TABLE:
            read_xref_table(sgl);
            break;
        case XREF_TYPE_STREAM:
            return ERR_NOT_IMPLEMENTED; // TODO
        default:
            return ERR_PDF_CONTENT;
    }

    return ERR_NONE;
}

void print_xref(xref_t *xref)
{
    xref_entry_t *xref_entry;
    if (xref == NULL)
        return;

    printf("\nXREF\n");
    for (size_t i = 0; i < xref->capacity; i++) {
        xref_entry = xref->entry[i];

        while (xref_entry != NULL) {
            printf("obj %zd (gen %zd) | offset %zd\n", i,
                   xref_entry->generation_num, xref_entry->byte_offset);
            xref_entry = xref_entry->next;
        }
    }
}

int sigil_xref_self_test(int verbosity)
{
    sigil_t *sgl = NULL;

    print_module_name("xref", verbosity);

    // TEST: fn determine_xref_type - TABLE
    print_test_item("fn determine_xref_type TABLE", verbosity);

    {
        sgl = test_prepare_sgl_path(
            "test/uznavany_bez_razitka_bez_revinfo_27_2_2012_CMS.pdf");
        if (sgl == NULL)
            goto failed;

        sgl->xref_type = XREF_TYPE_UNSET;

        if (pdf_move_pos_abs(sgl, 67954) != ERR_NONE)
            goto failed;

        if (determine_xref_type(sgl) != ERR_NONE ||
            sgl->xref_type != XREF_TYPE_TABLE)
        {
            goto failed;
        }

        sigil_free(&sgl);
    }

    print_test_result(1, verbosity);

    // TEST: fn determine_xref_type - STREAM
    print_test_item("fn determine_xref_type STREAM", verbosity);

    {
        sgl = test_prepare_sgl_path(
            "test/SampleSignedPDFDocument.pdf");
        if (sgl == NULL)
            goto failed;

        sgl->xref_type = XREF_TYPE_UNSET;

        if (pdf_move_pos_abs(sgl, 116) != ERR_NONE)
            goto failed;

        if (determine_xref_type(sgl) != ERR_NONE ||
            sgl->xref_type != XREF_TYPE_STREAM)
        {
            goto failed;
        }

        sigil_free(&sgl);
    }

    print_test_result(1, verbosity);

    // TEST: fn read_startxref
    print_test_item("fn read_startxref", verbosity);

    {
        char *sstream = "abcdefghi\n" \
                        "startxref\n" \
                        "1234567890\n"\
                        "\045\045EOF"; // %%EOF
        if ((sgl = test_prepare_sgl_buffer(sstream, strlen(sstream) + 1)) == NULL)
            goto failed;

        if (read_startxref(sgl) != ERR_NONE ||
            sgl->offset_startxref != 1234567890)
        {
            goto failed;
        }

        sigil_free(&sgl);
    }

    print_test_result(1, verbosity);

    // all tests done
    print_module_result(1, verbosity);

    return 0;

failed:
    if (sgl)
        sigil_free(&sgl);

    print_test_result(0, verbosity);
    print_module_result(0, verbosity);

    return 1;
}
