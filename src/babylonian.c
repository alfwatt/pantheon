/*
    babylonian.c — Babylonian cuneiform numerals in UTF-8.

    Babylonian scribes pressed numbers into wet clay with a reed,
    using just two wedge shapes:

    𒁹  U+12079 CUNEIFORM SIGN DISH  (the vertical wedge) = 1
    𒌋  U+1230B CUNEIFORM SIGN U      (the corner wedge)   = 10

    Each base-60 place (0 to 59) is written as some corner wedges
    for the tens, then some vertical wedges for the ones: 23 is two
    U signs and three DISH signs. Places are separated by one space
    (shown here as ␣):

    23          = 𒌋𒌋𒁹𒁹𒁹
    8583        = 𒁹𒁹␣𒌋𒌋𒁹𒁹𒁹␣𒁹𒁹𒁹       (2,23,3: 2 x 3600 + 23 x 60 + 3)
    1;24,51,10  = 𒁹␣𒌋𒌋𒁹𒁹𒁹𒁹␣𒌋𒌋𒌋𒌋𒌋𒁹␣𒌋  (the square root of 2 on
                                             the tablet YBC 7289)

    The Babylonians had no zero sign for most of their history. An
    empty place was just a gap, so here it is written as two spaces:
    3601 (1,0,1) = 𒁹␣␣␣␣𒁹, the separator, the gap, and the separator.
    That brings back a real problem the scribes had: a lone DISH
    could mean 1, 60, or 1/60, and only the context told the reader
    which.

    The code spells the signs as \U escapes so the exact code point
    is visible; the comments show the signs themselves. Unicode also has a "Cuneiform Numbers and
    Punctuation" block (U+12400 and up), but those signs are for
    weights and measures, not the place-value digits used here.

    Further reading:
    - https://en.wikipedia.org/wiki/Babylonian_numerals
    - https://en.wikipedia.org/wiki/Cuneiform
    - https://en.wikipedia.org/wiki/0#History_of_the_number_zero
*/
#include <assert.h>
#include <string.h>

#include "pantheon.h"

#define DISH_UTF8  "\U00012079" /* bytes: F0 92 81 B9 */
#define U_UTF8     "\U0001230B" /* bytes: F0 92 8C 8B */
#define SIGN_BYTES 4            /* both signs are 4 UTF-8 bytes */
#define GAP_BYTES  2            /* an empty place: two spaces */

/*
    A value is well-formed when it has 1 to MAX places, every place
    is at most 59, and the radix is between 0 and n_places.
*/
static int well_formed(pn_base60 v) {
    if (v.n_places == 0 || v.n_places > PN_BASE60_MAX_PLACES) {
        return 0;
    }
    if (v.radix > v.n_places) {
        return 0;
    }
    for (unsigned i = 0; i < v.n_places; i++) {
        if (v.places[i] > 59) {
            return 0;
        }
    }
    return 1;
}

/* How many bytes one place's wedges take; a 0 place is the two-space gap. */
static size_t group_bytes(unsigned d) {
    return (d == 0) ? GAP_BYTES : (d / 10u + d % 10u) * SIGN_BYTES;
}

size_t pn_babylonian_render_size(pn_base60 v) {
    size_t n = v.neg ? 1u : 0u;

    for (unsigned i = 0; i < v.n_places; i++) {
        if (i > 0) {
            n += 1u; /* one space between places */
        }
        n += group_bytes(v.places[i]);
    }
    return n;
}

char* _Nullable pn_babylonian_render(pn_base60 v, char* _Nonnull buf, size_t buflen,
                                     pn_error* _Nullable* _Nullable error) {
    assert(buf);

    if (!well_formed(v)) {
        if (error) {
            *error = pn_error_init(PN_ERROR_DOMAIN,
                                   "the value is not well-formed:"
                                   " %u places, radix %u - places need"
                                   " 1 to %u slots, each 0 to 59, and"
                                   " the radix cannot pass n_places",
                                   v.n_places, v.radix, PN_BASE60_MAX_PLACES);
        }
        return NULL;
    }

    /* render_size is exact, so buflen must hold that many bytes plus the 0 byte */
    size_t need = pn_babylonian_render_size(v);

    if (buflen <= need) {
        if (error) {
            *error = pn_error_init(PN_ERROR_BUFFER,
                                   "buffer holds %zu bytes; this value"
                                   " needs %zu plus the terminating NUL",
                                   buflen, need);
        }
        return NULL;
    }

    char* q = buf; /* safe: the size was checked above */

    if (v.neg) {
        *q++ = '-'; /* a modern addition: tablets had no minus sign */
    }
    for (unsigned i = 0; i < v.n_places; i++) {
        if (i > 0) {
            *q++ = ' ';
        }
        unsigned d = v.places[i];

        if (d == 0) {
            *q++ = ' ';
            *q++ = ' '; /* an empty place: the gap a scribe left */
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

    return buf;
}

/*
    Reading is the reverse of writing. The text is groups of wedges
    separated by runs of spaces, so the reader splits it into those
    two kinds of piece. Each empty place adds two spaces, so the
    number of spaces in a run tells how many empty places it holds:

     before the first group   3k      (k empty places)
     between two groups        3k+1    (one separator + k empty places)
     after the last group     3k      (k empty places)
     only spaces               3k-1    (k empty places, no groups)

    Anything else (other bytes, other run lengths, wedges out of
    order) is rejected. Being this strict is what guarantees that
    reading back written text gives the original value exactly.
*/
pn_base60* _Nullable pn_babylonian_parse_from(const char* _Nonnull buffer, size_t buflen,
                                              pn_base60* _Nonnull out,
                                              pn_error* _Nullable* _Nullable error) {
    assert(buffer && out);

    pn_base60 v; /* built here; *out changes only if the whole parse succeeds */

    memset(&v, 0, sizeof v);

    const char* p = buffer;
    const char* end = buffer + buflen;

    if (buflen > 0 && *p == '-') {
        v.neg = true;
        p++;
    }

    size_t spaces = 0; /* spaces seen since the last group */
    int got_group = 0;
    size_t n = 0; /* places stored so far */

    while (p < end) {
        if (*p == ' ') {
            const char* s0 = p;

            while (p < end && *p == ' ') {
                p++;
            }
            spaces += (size_t) (p - s0);
            continue;
        }

        /* A group starts here: first account for the spaces before it. */
        size_t zeros;

        if (!got_group) { /* before the first group: a multiple of 3 */
            if (spaces % 3u != 0u) {
                if (error) {
                    *error = pn_error_init(PN_ERROR_SYNTAX,
                                           "a string may open with %"
                                           "zu space(s), which is no"
                                           " whole row of empty places"
                                           " (an empty place costs 3)",
                                           spaces);
                }
                return NULL;
            }
            zeros = spaces / 3u;
        }
        else { /* between groups: 3k + 1 */
            if ((spaces - 1u) % 3u != 0u) {
                if (error) {
                    *error = pn_error_init(PN_ERROR_SYNTAX,
                                           "%zu spaces between groups:"
                                           " groups join with 1 space"
                                           " plus rows of empty places"
                                           " at 3 spaces each",
                                           spaces);
                }
                return NULL;
            }
            zeros = (spaces - 1u) / 3u;
        }

        if (n + zeros > PN_BASE60_MAX_PLACES) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "the empty places here push the"
                                       " value past its %u place limit",
                                       PN_BASE60_MAX_PLACES);
            }
            return NULL;
        }

        for (; zeros > 0; zeros--) {
            v.places[n++] = 0;
        }

        /*
            The group itself: U signs first, then DISH signs. At most
            5 U (50) and 9 DISH (9), since 59 is the biggest place.
            A sixth U would be 60 or more, too big for one place.
        */
        size_t nu = 0, nd = 0;

        while (p + SIGN_BYTES <= end && memcmp(p, U_UTF8, SIGN_BYTES) == 0) {
            p += SIGN_BYTES;
            if (++nu > 5u) {
                if (error) {
                    *error = pn_error_init(PN_ERROR_RANGE, "six corner wedges in one"
                                                           " group: a place holds at"
                                                           " most five tens");
                }
                return NULL;
            }
        }

        while (p + SIGN_BYTES <= end && memcmp(p, DISH_UTF8, SIGN_BYTES) == 0) {
            p += SIGN_BYTES;
            if (++nd > 9u) {
                if (error) {
                    *error = pn_error_init(PN_ERROR_RANGE, "ten vertical wedges in one"
                                                           " group: a scribe would"
                                                           " carry to the next place");
                }
                return NULL;
            }
        }

        if (nu + nd == 0u) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "byte at position %zu starts no"
                                       " wedge sign (or cuts one off"
                                       " with %zu byte(s) left)",
                                       (size_t) (p - buffer), (size_t) (end - p));
            }
            return NULL;
        }

        if (p != end && *p != ' ') {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "byte %zu glues a wedge onto the"
                                       " group without a space between",
                                       (size_t) (p - buffer));
            }
            return NULL;
        }

        v.places[n++] = (uint8_t) (nu * 10u + nd);
        got_group = 1;
        spaces = 0;
    }

    if (!got_group) { /* only spaces: 3k-1 spaces means k empty places */
        if ((spaces + 1u) % 3u != 0u) {
            if (error) {
                *error =
                    pn_error_init(PN_ERROR_SYNTAX,
                                  "%zu space(s) with no wedge in"
                                  " sight name no value",
                                  spaces); /* the empty string and a lone "-" end up here too */
            }
            return NULL;
        }
        size_t k = (spaces + 1u) / 3u;

        if (k > PN_BASE60_MAX_PLACES) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "%zu empty places will not fit"
                                       " the value",
                                       k);
            }
            return NULL;
        }

        for (; k > 0; k--) {
            v.places[n++] = 0;
        }
    }
    else if (spaces % 3u != 0u) { /* after the last group: a multiple of 3 */
        if (error) {
            *error = pn_error_init(PN_ERROR_SYNTAX,
                                   "the string trails off with %zu"
                                   " space(s): a blank tail comes"
                                   " in rows of 3",
                                   spaces);
        }
        return NULL;
    }
    else {
        size_t zeros = spaces / 3u;

        if (n + zeros > PN_BASE60_MAX_PLACES) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "the blank tail pushes the"
                                       " value past its %u place"
                                       " limit",
                                       PN_BASE60_MAX_PLACES);
            }
            return NULL;
        }

        for (; zeros > 0; zeros--) {
            v.places[n++] = 0;
        }
    }

    if (n == 0) {
        if (error) {
            *error = pn_error_init(PN_ERROR_SYNTAX, "no place survived the reading"
                                                    " (unreachable; belt and"
                                                    " tablet-clay)");
        }
        return NULL;
    }

    v.n_places = (uint8_t) n;
    v.radix = (uint8_t) n; /* clay tablets show no radix point, so read it as a whole number */
    *out = v;

    return out;
}
