/*
    mayan.c — Maya bar-and-dot numerals in UTF-8.

    The Maya wrote each base-20 place as a stack of dots (ones) and
    bars (fives): 13 is two bars and three dots. They also had a
    real zero sign, usually drawn as a shell. Unicode has one
    ready-made sign for each place value 0 to 19:

    𝋠 𝋡 𝋢 ... 𝋳   U+1D2E0..U+1D2F3  MAYAN NUMERAL ZERO..NINETEEN
                    UTF-8: F0 9D 8B, then A0..B3

    13 is 𝋭 (two bars, three dots) and 0 is 𝋠 (the shell).

    So this file is simpler than babylonian.c: one sign per place
    and one space between places. Because there is a zero sign, an
    empty place is never ambiguous, unlike the Babylonian gap.

    The Maya stacked places vertically with the smallest at the
    bottom. This module writes them left to right with the biggest
    first, the way we write decimal numbers (␣ is a space):

    429   = 𝋡␣𝋡␣𝋩       (1 x 400 + 1 x 20 + 9)
    8800  = 𝋡␣𝋢␣𝋠␣𝋠    (1 x 8000 + 2 x 400 + 0 + 0)

    The code spells the signs as \U escapes so the exact code point
    is visible; the comments show the signs themselves.

    Further reading:
    - https://en.wikipedia.org/wiki/Maya_numerals
    - https://www.unicode.org/charts/PDF/U1D2E0.pdf (the Unicode chart)
*/
#include <assert.h>
#include <string.h>

#include "pantheon.h"

#define MAYAN_PREFIX "\xF0\x9D\x8B" /* F0 9D 8B: the first 3 bytes of all 20 signs */
#define MAYAN_LO     0xA0           /* last byte of U+1D2E0, zero (the shell)      */
#define MAYAN_HI     0xB3           /* last byte of U+1D2F3, nineteen              */
#define SIGN_BYTES   4              /* each sign is 4 bytes in UTF-8               */

/* digit_signs[d] is the sign for d, which is U+1D2E0 + d. */
static const char* const digit_signs[20] = {
    "\U0001D2E0", "\U0001D2E1", "\U0001D2E2", "\U0001D2E3", "\U0001D2E4",
    "\U0001D2E5", "\U0001D2E6", "\U0001D2E7", "\U0001D2E8", "\U0001D2E9",
    "\U0001D2EA", "\U0001D2EB", "\U0001D2EC", "\U0001D2ED", "\U0001D2EE",
    "\U0001D2EF", "\U0001D2F0", "\U0001D2F1", "\U0001D2F2", "\U0001D2F3",
};

/*
    A value is well-formed when it has 1 to MAX places, every place
    is at most 19, and the radix is between 0 and n_places. Each
    file checks this itself (base20.c and babylonian.c have their own
    copies).
*/
static int well_formed(pn_base20 v) {
    if (v.n_places == 0 || v.n_places > PN_BASE20_MAX_PLACES) {
        return 0;
    }
    if (v.radix > v.n_places) {
        return 0;
    }
    for (unsigned i = 0; i < v.n_places; i++) {
        if (v.places[i] > 19) {
            return 0;
        }
    }
    return 1;
}

/*
    Read one digit sign at p (at least SIGN_BYTES bytes must remain).
    Returns the digit 0 to 19 in *digit, or -1 if the bytes are not
    one of the twenty signs. U+1D2F4 to U+1D2FF are in the same
    Unicode block but unassigned, so they return -1 too.
*/
static int match_mayan(const char* p, const char* end, unsigned* digit) {
    if (p + SIGN_BYTES > end || memcmp(p, MAYAN_PREFIX, 3) != 0) {
        return -1;
    }
    const unsigned char b = (unsigned char) p[3];

    if (b >= MAYAN_LO && b <= MAYAN_HI) { /* U+1D2E0 to U+1D2F3 */
        *digit = (unsigned) (b - MAYAN_LO);
        return 0;
    }
    return -1;
}

size_t pn_mayan_render_size(pn_base20 v) {
    /*
        Never fails: '-' if negative, one sign per place, and one
        space between places. For a well-formed v this is exactly
        what render writes. For a malformed v the number means
        nothing, and render reports PN_ERROR_DOMAIN.
    */
    size_t n = v.neg ? 1u : 0u;

    for (unsigned i = 0; i < v.n_places; i++) {
        if (i > 0) {
            n += 1u; /* one space between places */
        }
        n += SIGN_BYTES;
    }
    return n;
}

char* _Nullable pn_mayan_render(pn_base20 v, char* _Nonnull buf, size_t buflen,
                                pn_error* _Nullable* _Nullable error) {
    assert(buf);
    if (!well_formed(v)) {
        if (error) {
            *error = pn_error_init(PN_ERROR_DOMAIN,
                                   "the value is not well-formed:"
                                   " %u places, radix %u - places need"
                                   " 1 to %u slots, each 0 to 19, and"
                                   " the radix cannot pass n_places",
                                   v.n_places, v.radix, PN_BASE20_MAX_PLACES);
        }
        return NULL;
    }

    /* render_size is exact, so buflen must hold that many bytes plus the null terminator */
    const size_t need = pn_mayan_render_size(v);

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
        *q++ = '-'; /* a modern addition: the Maya had no minus sign */
    }
    for (unsigned i = 0; i < v.n_places; i++) {
        if (i > 0) {
            *q++ = ' ';
        }
        memcpy(q, digit_signs[v.places[i]], SIGN_BYTES);
        q += SIGN_BYTES;
    }
    *q = '\0';
    return buf;
}

/*
    Reading is the reverse of writing. The format is an optional
    '-', then signs separated by single spaces. A zero sign can be in
    any place, including the first, and is kept as written. More
    than PN_BASE20_MAX_PLACES places is PN_ERROR_RANGE (too big),
    not a syntax error.
*/
pn_base20 pn_mayan_parse(const char* _Nonnull text, pn_error* _Nullable* _Nullable error) {
    assert(text);

    size_t text_len = strlen(text);
    if (text_len == 0) {
        if (error) {
            *error = pn_error_init(PN_ERROR_SYNTAX, "an empty tablet is no number:"
                                                    " the shell is how zero spells"
                                                    " itself");
        }
        return (pn_base20) {0};
    }

    const char *p = text, *end = text + text_len;

    pn_base20 v; /* built here; returned only if the whole parse succeeds */

    memset(&v, 0, sizeof v);

    if (*p == '-') {
        if (p + 1 >= end) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX, "a '-' sitting over nothing");
            }
            return (pn_base20) {0};
        }
        v.neg = true;
        p++;
    }

    unsigned k = 0;

    for (;;) {
        unsigned digit;

        if (match_mayan(p, end, &digit) != 0) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "%zu byte(s) at position %zu"
                                       " are neither a whole digit"
                                       " sign nor a single joining"
                                       " space (doubled gutter or"
                                       " two signs touching look"
                                       " the same from here)",
                                       (size_t) (end - p), (size_t) (p - text));
            }
            return (pn_base20) {0};
        }
        if (k == PN_BASE20_MAX_PLACES) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "the row past the page:"
                                       " a codex stack holds %u",
                                       PN_BASE20_MAX_PLACES);
            }
            return (pn_base20) {0};
        }
        v.places[k++] = (uint8_t) digit;
        p += SIGN_BYTES;
        if (p == end) {
            break;
        }
        if (*p != ' ' || p + 1 >= end) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "rows join with one space:"
                                       " position %zu wears a"
                                       "%s",
                                       (size_t) (p - text),
                                       (*p == ' ') ? " trailing"
                                                     " space with no row under"
                                                     " it"
                                                   : " no gutter");
            }
            return (pn_base20) {0};
        }
        p++;
    }
    v.n_places = (uint8_t) k;
    /*
        the signs show no radix point, so read it as a whole number, the same as the cuneiform
        reader does
    */
    v.radix = v.n_places;
    return v;
}
