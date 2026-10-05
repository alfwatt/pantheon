/*  error.c — libPantheon: the note a failed call leaves behind.

    A success allocates nothing.  A failure is allowed one malloc:
    the pn_error note that explains the failure, which the caller
    frees.  These few functions build, print, compare, and destroy
    those notes; the numeral modules call them and never touch
    malloc themselves.
*/
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pantheon.h"

/* A private copy: strdup is not in the ISO C set this library builds
   under, so copy the bytes by hand. */
static char *pn_error_text_copy(const char *text)
{
    size_t len = strlen(text) + 1u;
    char *copy = malloc(len);

    if (copy == NULL)
        abort(); /* a failure to report a failure has no second voice */
    memcpy(copy, text, len);
    return copy;
}

/* The message is built with printf into a counted buffer: vsnprintf
   once to learn the length, once more to fill it. */
static char *pn_error_message_copy(const char *format, va_list args)
{
    va_list copy;
    int room;
    char *text;

    va_copy(copy, args);
    room = vsnprintf(NULL, 0u, format, copy);
    va_end(copy);
    if (room < 0)
        return NULL;

    text = malloc((size_t)room + 1u);
    if (text == NULL)
        return NULL;
    vsnprintf(text, (size_t)room + 1u, format, args);
    return text;
}

pn_error *pn_error_init(pn_error_code code, const char *message)
{
    pn_error *error = malloc(sizeof *error);

    if (error == NULL)
        abort(); /* a failure to report a failure has no second voice */
    error->code = code;
    error->message = (message != NULL) ? pn_error_text_copy(message) : NULL;
    error->prior = NULL;
    return error;
}

pn_error *pn_error_prepend_note(pn_error *prior, pn_error_code code,
                                const char *message)
{
    pn_error *error = pn_error_init(code, message);

    error->prior = prior; /* the new note owns the old one */
    return error;
}

bool pn_error_set(pn_error **location, pn_error_code code,
                  const char *format, ...)
{
    va_list args;
    char *message;

    va_start(args, format);
    message = pn_error_message_copy(format, args);
    va_end(args);

    if (location != NULL) {
        pn_error_free(*location);
        *location = pn_error_prepend_note(NULL, code, message);
    }
    free(message);
    return false;
}

bool pn_error_prepend(pn_error **location, pn_error *cause,
                      pn_error_code code, const char *format, ...)
{
    va_list args;
    char *message;

    va_start(args, format);
    message = pn_error_message_copy(format, args);
    va_end(args);

    if (location != NULL) {
        pn_error_free(*location);
        *location = pn_error_prepend_note(cause, code, message);
    } else {
        pn_error_free(cause); /* no listener: nothing keeps the sentence */
    }
    free(message);
    return false;
}

void pn_error_free(pn_error *error)
{
    while (error != NULL) {
        pn_error *older = error->prior;

        free(error->message);
        free(error);
        error = older;
    }
}

bool pn_error_is(const pn_error *error, pn_error_code code)
{
    return error != NULL && error->code == code;
}

const char *pn_error_describe(const pn_error *error)
{
    static const char *const names[] = {
        "null",     /* PN_ERROR_NULL    */
        "syntax",   /* PN_ERROR_SYNTAX  */
        "range",    /* PN_ERROR_RANGE   */
        "domain",   /* PN_ERROR_DOMAIN  */
        "buffer"    /* PN_ERROR_BUFFER  */
    };

    if (error == NULL)
        return "no error";
    if (error->message != NULL)
        return error->message;
    if ((unsigned)error->code < sizeof names / sizeof names[0])
        return names[error->code];
    return "unknown error";
}

void pn_error_clear(pn_error **location)
{
    if (location != NULL) {
        pn_error_free(*location);
        *location = NULL;
    }
}
