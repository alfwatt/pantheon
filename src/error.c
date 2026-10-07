/*  error.c — libPantheon: the note a failed call leaves behind.

    A success allocates nothing.  A failure is allowed one malloc:
    one block holding the pn_error struct followed directly by its
    message text, which the caller frees with pn_error_free.  One
    block per note means one free per note.  These few functions
    hang, read, and destroy the notes; the numeral modules call them
    and never touch malloc themselves.

    Only the failing call itself hangs a note, through pn_error_set.
    It is the prerogative of a function to set its own out
    parameters: no builder here hands out finished notes for some
    other function to place on a caller's slot.

    A caller who does not want explanations passes a NULL slot: the
    builder then makes nothing at all and the call simply reports
    failure through its return value.
*/
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pantheon.h"

/* The one malloc a failure is allowed: one block holding the struct,
   then the message bytes packed right behind it with a NUL after
   them so the sentence can go straight into printf.  vsnprintf once
   with a NULL buffer to learn the length, then once more to fill the
   tail.  vsnprintf with a NULL buffer can only fail on a broken
   format string, which is a programmer's mistake: there is no second
   voice for that. */
static pn_error* pn_error_build_fmt(pn_error_code code, const char* format, va_list args) {
    va_list again;
    int room;
    pn_error* error;
    char* text;

    va_copy(again, args);
    room = vsnprintf(NULL, 0u, format, again);
    va_end(again);
    if (room < 0)
        abort(); /* a failure to report a failure has no second voice */

    error = malloc(sizeof *error + (size_t) room + 1u);
    if (error == NULL)
        abort(); /* a failure to report a failure has no second voice */
    error->code = code;
    text = (char*) (void*) (error + 1);
    vsnprintf(text, (size_t) room + 1u, format, args);
    error->message = text;
    return error;
}

/* The code's own name, for the note that has no sentence of its
   own. */
static const char* pn_error_code_name(pn_error_code code) {
    static const char* const names[] = {
        "null",   /* PN_ERROR_NULL    */
        "syntax", /* PN_ERROR_SYNTAX  */
        "range",  /* PN_ERROR_RANGE   */
        "domain", /* PN_ERROR_DOMAIN  */
        "buffer"  /* PN_ERROR_BUFFER  */
    };

    if ((unsigned) code < sizeof names / sizeof names[0])
        return names[code];
    return "unknown error";
}

bool pn_error_set(pn_error** location, pn_error_code code, const char* format, ...) {
    va_list args;
    pn_error* note;

    if (location == NULL)
        return false; /* no listener: the note is simply not made */

    va_start(args, format);
    note = pn_error_build_fmt(code, format, args);
    va_end(args);

    pn_error_free(*location); /* only the newest note survives */
    *location = note;
    return false;
}

void pn_error_free(pn_error* error) {
    free(error); /* the message rode in the same block */
}

bool pn_error_is(const pn_error* error, pn_error_code code) {
    return error != NULL && error->code == code;
}

/* The note's sentence for printf("%s", ...): its own message when it
   has one, the code's name when it does not, "no error" for a
   missing note.  The sentence belongs to the note: it stays alive
   exactly as long as the note does and needs no free of its own. */
const char* pn_error_describe(const pn_error* error) {
    if (error == NULL)
        return "no error";
    if (error->message != NULL)
        return error->message;
    return pn_error_code_name(error->code);
}
