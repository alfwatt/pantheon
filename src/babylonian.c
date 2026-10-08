/*  babylonian.c — libPantheon: positional cuneiform numerals, UTF-8.

    Two signs do all the work, the same pair pressed into the clay:

    U+12079 CUNEIFORM SIGN DISH  (the vertical wedge) = 1
    U+1230B CUNEIFORM SIGN U      (the corner wedge)   = 10

    A place worth d takes (d/10) copies of U followed by (d%10)
    copies of DISH, and places sit one space apart.  Unicode has no
    zero sign for the sexagesimal gap, so an empty place is written
    as two spaces - the very blank a scribe left, with the very
    ambiguity that came with it: on the tablet a lone DISH meant 1,
    60, or 1/60, and only context told the reader which.

    The signs are spelled as universal character names so this file
    stays pure ASCII.  One deliberate choice: the precomposed signs
    of Unicode's "Cuneiform Numbers and Punctuation" block
    (U+12400..) are NOT used - those are weights-and-measures signs,
    not the school-text digits of place-value writing.

    Reference: https://en.wikipedia.org/wiki/Babylonian_numerals
*/
#include <string.h>

#include "pantheon.h"

#define DISH_UTF8  "\U00012079" /* bytes: F0 92 81 B9 */
#define U_UTF8     "\U0001230B" /* bytes: F0 92 8C 8B */
#define SIGN_BYTES 4            /* both signs are 4 UTF-8 bytes */
#define GAP_BYTES  2            /* an empty place: two spaces */

/* A value is well-formed when it has 1 to MAX places, every place
   at most 59, and a radix between 0 and n_places. */
static int well_formed(pn_base60 v) {
    if (v.n_places == 0 || v.n_places > PN_BASE60_MAX_PLACES)
        return 0;
    if (v.radix > v.n_places)
        return 0;
    for (unsigned i = 0; i < v.n_places; i++)
        if (v.places[i] > 59)
            return 0;
    return 1;
}

/* How many bytes one place's wedges need; a place of 0 is the gap. */
static size_t group_bytes(unsigned d) {
    return (d == 0) ? GAP_BYTES : (d / 10u + d % 10u) * SIGN_BYTES;
}

size_t pn_babylonian_render_size(pn_base60 v) {
    size_t n = v.neg ? 1u : 0u;

    for (unsigned i = 0; i < v.n_places; i++) {
        if (i > 0)
            n += 1u; /* one space between places */
        n += group_bytes(v.places[i]);
    }
    return n;
}

bool pn_babylonian_render(pn_base60 v, char* _Nonnull buf, size_t buflen,
                          pn_error* _Nullable* _Nullable error) {
    if (buf == NULL)
        return pn_error_set(error, PN_ERROR_NULL, "render needs a buffer to write into");
    if (!well_formed(v))
        return pn_error_set(error, PN_ERROR_DOMAIN,
                            "the value is not well-formed:"
                            " %u places, radix %u - places need"
                            " 1 to %u slots, each 0 to 59, and"
                            " the radix cannot pass n_places",
                            v.n_places, v.radix, PN_BASE60_MAX_PLACES);

    /* render_size is exact: buflen has to hold its bytes plus NUL */
    size_t need = pn_babylonian_render_size(v);

    if (buflen <= need)
        return pn_error_set(error, PN_ERROR_BUFFER,
                            "buffer holds %zu bytes; this value"
                            " needs %zu plus the terminating NUL",
                            buflen, need);

    char* q = buf; /* safe: need is exact and already checked */

    if (v.neg)
        *q++ = '-'; /* an anachronism; tablets wore no sign */
    for (unsigned i = 0; i < v.n_places; i++) {
        if (i > 0)
            *q++ = ' ';
        unsigned d = v.places[i];

        if (d == 0) {
            *q++ = ' ';
            *q++ = ' '; /* the historical gap */
            continue;
        }
        for (unsigned j = 0; j < d / 10u; j++) {
            memcpy(q, U_UTF8, SIGN_BYTES);
            q += SIGN_BYTES;
        }
        for (unsigned j = 0; j < d % 10u; j++) {
            memcpy(q, DISH_UTF8, SIGN_BYTES);
            q += SIGN_BYTES;
        }
    }
    *q = '\0';
    return true;
}

/* Parsing reads back what rendering wrote.

    A rendered string is wedge groups held apart by runs of spaces,
   so the parser simply cuts the string into the two kinds of piece:
    runs of spaces, and runs of signs.  The renderer's spacing law
    decides what a legal string looks like:

    leading run     3k      (k empty places before the first group)
    between runs    3k+1    (one joiner space plus k two-space gaps)
    trailing run    3k      (k empty places after the last group)
    all-space       3k-1    (k empty places, no group at all)

    Every other shape - other bytes, other spacings, groups written
   out of order - is refused, and that strictness is exactly what
   makes the parse-after-render round trip exact. */
bool pn_babylonian_parse(const char* _Nullable buf, size_t buflen, pn_base60* _Nullable out,
                         pn_error* _Nullable* _Nullable error) {
    if (buf == NULL || out == NULL)
        return pn_error_set(error, PN_ERROR_NULL,
                            "parse needs a string and a place to"
                            " put the value");

    pn_base60 v; /* built here; *out keeps its old value unless all works */

    memset(&v, 0, sizeof v);

    const char *p = buf, *end = buf + buflen;

    if (buflen > 0 && *p == '-') {
        v.neg = true;
        p++;
    }

    size_t spaces = 0; /* the space run still pending */
    int got_group = 0;
    size_t n = 0; /* places stored so far */

    while (p < end) {
        if (*p == ' ') {
            const char* s0 = p;

            while (p < end && *p == ' ')
                p++;
            spaces += (size_t) (p - s0);
            continue;
        }

        /* A group starts here: hand over the spaces still pending. */
        size_t zeros;

        if (!got_group) { /* leading run: a multiple of 3 */
            if (spaces % 3u != 0u)
                return pn_error_set(error, PN_ERROR_SYNTAX,
                                    "a string may open with %"
                                    "zu space(s), which is no"
                                    " whole row of empty places"
                                    " (an empty place costs 3)",
                                    spaces);
            zeros = spaces / 3u;
        }
        else { /* between runs: 3k + 1 */
            if ((spaces - 1u) % 3u != 0u)
                return pn_error_set(error, PN_ERROR_SYNTAX,
                                    "%zu spaces between groups:"
                                    " groups join with 1 space"
                                    " plus rows of empty places"
                                    " at 3 spaces each",
                                    spaces);
            zeros = (spaces - 1u) / 3u;
        }
        if (n + zeros > PN_BASE60_MAX_PLACES)
            return pn_error_set(error, PN_ERROR_RANGE,
                                "the empty places here push the"
                                " value past its %u place limit",
                                PN_BASE60_MAX_PLACES);
        for (; zeros > 0; zeros--)
            v.places[n++] = 0;

        /*  The group itself: U signs first, then DISH signs, in
  canonical order with canonical counts only (five tens
  plus nine ones still sits under 60; a sixth ten is no
  single place).
*/
        size_t nu = 0, nd = 0;

        while (p + SIGN_BYTES <= end && memcmp(p, U_UTF8, SIGN_BYTES) == 0) {
            p += SIGN_BYTES;
            if (++nu > 5u)
                return pn_error_set(error, PN_ERROR_RANGE,
                                    "six corner wedges in one"
                                    " group: a place holds at"
                                    " most five tens");
        }
        while (p + SIGN_BYTES <= end && memcmp(p, DISH_UTF8, SIGN_BYTES) == 0) {
            p += SIGN_BYTES;
            if (++nd > 9u)
                return pn_error_set(error, PN_ERROR_RANGE,
                                    "ten vertical wedges in one"
                                    " group: a scribe would"
                                    " carry to the next place");
        }
        if (nu + nd == 0u)
            return pn_error_set(error, PN_ERROR_SYNTAX,
                                "byte at position %zu starts no"
                                " wedge sign (or cuts one off"
                                " with %zu byte(s) left)",
                                (size_t) (p - buf), (size_t) (end - p));
        if (p != end && *p != ' ')
            return pn_error_set(error, PN_ERROR_SYNTAX,
                                "byte %zu glues a wedge onto the"
                                " group without a space between",
                                (size_t) (p - buf));
        v.places[n++] = (uint8_t) (nu * 10u + nd);
        got_group = 1;
        spaces = 0;
    }

    if (!got_group) { /* all-space: 3k-1 spaces, k empty places */
        if ((spaces + 1u) % 3u != 0u)
            return pn_error_set(error, PN_ERROR_SYNTAX,
                                "%zu space(s) with no wedge in"
                                " sight name no value",
                                spaces); /* "" and "-" land here too */
        size_t k = (spaces + 1u) / 3u;

        if (k > PN_BASE60_MAX_PLACES)
            return pn_error_set(error, PN_ERROR_RANGE,
                                "%zu empty places will not fit"
                                " the value",
                                k);
        for (; k > 0; k--)
            v.places[n++] = 0;
    }
    else if (spaces % 3u != 0u) { /* trailing run: a multiple of 3 */
        return pn_error_set(error, PN_ERROR_SYNTAX,
                            "the string trails off with %zu"
                            " space(s): a blank tail comes"
                            " in rows of 3",
                            spaces);
    }
    else {
        size_t zeros = spaces / 3u;

        if (n + zeros > PN_BASE60_MAX_PLACES)
            return pn_error_set(error, PN_ERROR_RANGE,
                                "the blank tail pushes the"
                                " value past its %u place"
                                " limit",
                                PN_BASE60_MAX_PLACES);
        for (; zeros > 0; zeros--)
            v.places[n++] = 0;
    }

    if (n == 0)
        return pn_error_set(error, PN_ERROR_SYNTAX,
                            "no place survived the reading"
                            " (unreachable; belt and"
                            " tablet-clay)");

    v.n_places = (uint8_t) n;
    v.radix = (uint8_t) n; /* no radix point on clay: whole numbers */
    *out = v;
    return true;
}
