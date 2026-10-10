/*
    error.c — error reports for libPantheon.

    When a function fails, it can hand back a pn_error: a code that
    names the kind of failure plus a sentence that explains it. This
    file builds, reads and frees those reports.

    Successful calls never allocate memory. A failure allocates
    exactly once: one block that holds the pn_error struct with its
    message text stored right after it. Because it is one block, the
    caller frees it with one call to pn_error_free.

    Every failing function in the library follows the same pattern:

        if (error) {
            *error = pn_error_init(PN_ERROR_X, "what went wrong");
        }
        return NULL;

    A caller that does not need the explanation passes NULL for the
    error pointer. Then no report is built, and the caller learns
    about the failure only from the return value: NULL for a pointer,
    or a zeroed value for a struct.

    C has no exceptions, so returning an error value like this is the
    usual way to report failure in C.

    Further reading:
    - https://en.wikipedia.org/wiki/Exception_handling
*/
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pantheon.h"

/*
    Build one error report in a single malloc: the struct first, then
    the message text right after it, null terminated so it can be
    printed directly. vsnprintf runs twice: once with no buffer, to
    measure how long the message is, then again to write it. The
    first call fails only if the format string itself is broken,
    which is a bug in this library, so the program stops.
*/
static pn_error* pn_error_build_fmt(pn_error_code code, const char* format, va_list args) {
    va_list again;
    int room;
    pn_error* error;
    char* text;

    va_copy(again, args);
    room = vsnprintf(NULL, 0u, format, again);
    va_end(again);
    if (room < 0) {
        abort(); /* there is no way to report a failure here */
    }

    error = malloc(sizeof *error + (size_t) room + 1u);
    if (error == NULL) {
        abort(); /* there is no way to report a failure here */
    }
    error->code = code;
    text = (char*) (void*) (error + 1);
    vsnprintf(text, (size_t) room + 1u, format, args);
    error->message = text;
    return error;
}

/*
    A short name for each error code, used when a report has no
    message of its own.
*/
static const char* pn_error_code_name(pn_error_code code) {
    static const char* const names[] = {
        "null",   /* PN_ERROR_NULL    */
        "syntax", /* PN_ERROR_SYNTAX  */
        "range",  /* PN_ERROR_RANGE   */
        "domain", /* PN_ERROR_DOMAIN  */
        "buffer"  /* PN_ERROR_BUFFER  */
    };

    if ((unsigned) code < sizeof names / sizeof names[0]) {
        return names[code];
    }
    return "unknown error";
}

pn_error* pn_error_init(pn_error_code code, const char* format, ...) {
    va_list args;
    pn_error* note;

    va_start(args, format);
    note = pn_error_build_fmt(code, format, args);
    va_end(args);

    return note;
}

void pn_error_free(pn_error* error) {
    free(error); /* the message is in the same block, so one free covers both */
}

/*
    Text to print for an error report: its message if it has one, the
    code's name if not, and "no error" for NULL. The text lives inside
    the report, so read it before freeing the report.
*/
const char* pn_error_describe(const pn_error* error) {
    if (error == NULL) {
        return "no error";
    }
    if (error->message != NULL) {
        return error->message;
    }
    return pn_error_code_name(error->code);
}
