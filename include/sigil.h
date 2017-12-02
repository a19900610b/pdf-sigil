#ifndef PDF_SIGIL_SIGIL_H
#define PDF_SIGIL_SIGIL_H

#include <stdlib.h>
#include "error.h"

#ifndef CHAR_T
#define CHAR_T
    typedef char char_t;
#endif /* CHAR_T */


#define MODE_UNSET     0
#define MODE_VERIFY    1
#define MODE_SIGN      2

typedef uint32_t mode_t;

typedef struct {
    FILE   *file;
    char_t *filepath;
    mode_t  mode;
    short   pdf_x,             /* numbers from PDF header */
            pdf_y;             /*   %PDF-<pdf_x>.<pdf_y>  */
    size_t  file_size;
    size_t  pdf_start_offset;  /* offset of %PDF-x.y      */
    size_t  startxref;
} sigil_t;

sigil_err_t sigil_init(sigil_t **sgl);

sigil_err_t sigil_setup_file(sigil_t *sgl, const char_t *filepath);

sigil_err_t sigil_setup_mode(sigil_t *sgl, mode_t mode);

sigil_err_t sigil_process(sigil_t *sgl);

// ... get functions

void sigil_free(sigil_t *sgl);

int sigil_sigil_self_test(int verbosity);

#endif /* PDF_SIGIL_SIGIL_H */
