/*  egyptian.c — Egyptian hieroglyphic numerals in UTF-8.

    Ancient Egypt (from about 3000 BCE) wrote numbers with seven
    signs, one for each power of ten, repeating each sign as many
    times as its digit says.  It is a decimal system with no place
    values: the signs look different, so their position doesn't
    matter for the value.

    𓏤  U+133E4 EGYPTIAN HIEROGLYPH Z001  (single stroke)   = 1
    𓎆  U+13386 EGYPTIAN HIEROGLYPH V020  (cattle hobble)   = 10
    𓍢  U+13362 EGYPTIAN HIEROGLYPH V001  (coil of rope)    = 100
    𓆼  U+131BC EGYPTIAN HIEROGLYPH M012  (lotus)           = 1 000
    𓂭  U+130AD EGYPTIAN HIEROGLYPH D050  (bent finger)     = 10 000
    𓆐  U+13190 EGYPTIAN HIEROGLYPH I008  (tadpole)         = 100 000
    𓁨  U+13068 EGYPTIAN HIEROGLYPH C011  (Heh god)         = 1 000 000

    This module writes the signs biggest first, each sign in one
    group, with no spaces:

    4622     = 𓆼𓆼𓆼𓆼𓍢𓍢𓍢𓍢𓍢𓍢𓎆𓎆𓏤𓏤
    1111111  = 𓁨𓆐𓂭𓆼𓍢𓎆𓏤

    There is no sign above one million and no sign for zero (a
    scribe just left a blank), so 0 is the empty string and the
    largest value is 9 999 999, nine of every sign.

    The code spells the signs as \U escapes so the exact code point
    is visible; the comments show the signs themselves.

    Further reading: https://en.wikipedia.org/wiki/Egyptian_numerals
      https://en.wikipedia.org/wiki/UTF-8
*/
#include <assert.h>
#include <string.h>

#include "pantheon.h"

#define STROKE_UTF8  "\U000133E4" /* 𓏤 bytes: F0 93 8F A4 */
#define HOBBLE_UTF8  "\U00013386" /* 𓎆 bytes: F0 93 8E 86 */
#define COIL_UTF8    "\U00013362" /* 𓍢 bytes: F0 93 8D A2 */
#define LOTUS_UTF8   "\U000131BC" /* 𓆼 bytes: F0 93 86 BC */
#define FINGER_UTF8  "\U000130AD" /* 𓂭 bytes: F0 93 82 AD */
#define TADPOLE_UTF8 "\U00013190" /* 𓆐 bytes: F0 93 86 90 */
#define HEH_UTF8     "\U00013068" /* 𓁨 bytes: F0 93 81 A8 */
#define SIGN_BYTES   4            /* every sign is 4 UTF-8 bytes */

#define N_SIGNS 7 /* powers 10^0 through 10^6; nothing above the Heh */

/* Biggest sign first, the order they are written in. */
static const char* const signs[N_SIGNS] = {HEH_UTF8,  TADPOLE_UTF8, FINGER_UTF8, LOTUS_UTF8,
                                           COIL_UTF8, HOBBLE_UTF8,  STROKE_UTF8};

/* The value of each sign in the table above. */
static const uint64_t values[N_SIGNS] = {1000000u, 100000u, 10000u, 1000u, 100u, 10u, 1u};

size_t pn_egyptian_render_size(uint64_t n) {
    /*  Never fails.  For n up to PN_EGYPTIAN_MAX_VALUE the answer is
            exactly 4 bytes times the sum of n's decimal digits.  Above
            that the number is meaningless, and pn_egyptian_render
            reports PN_ERROR_RANGE.
        */
    size_t bytes = 0;

    while (n != 0) {
        bytes += (size_t) (n % 10u) * SIGN_BYTES;
        n /= 10u;
    }
    return bytes;
}

char* _Nullable pn_egyptian_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                                   pn_error* _Nullable* _Nullable error) {
    assert(buf);
    if (n > PN_EGYPTIAN_MAX_VALUE) {
        /*  No sign is bigger than the Heh (one million), so a digit in
                    the ten-millions place or above cannot be written.
                */
        if (error) {
            *error = pn_error_init(PN_ERROR_RANGE,
                                   "%llu needs ten or more signs of one kind;"
                                   " the biggest sign is the Heh at 1000000",
                                   (unsigned long long) n);
        }
        return NULL;
    }

    /* render_size is exact, so buflen must also hold the 0 byte */
    size_t need = pn_egyptian_render_size(n);
    if (buflen <= need) {
        if (error) {
            *error = pn_error_init(PN_ERROR_BUFFER,
                                   "buffer holds %zu bytes; %llu needs %zu plus"
                                   " the terminating NUL",
                                   buflen, (unsigned long long) n, need);
        }
        return NULL;
    }

    char* q = buf; /* safe: the size was checked above */
    for (unsigned i = 0; i < N_SIGNS; i++) {
        uint64_t digit = (n / values[i]) % 10u;

        for (uint64_t j = 0; j < digit; j++) {
            memcpy(q, signs[i], SIGN_BYTES);
            q += SIGN_BYTES;
        }
    }
    *q = '\0'; /* for n == 0 this writes only the 0 byte: an empty string */
    return buf;
}

/* Reading is the reverse of writing, and only the standard form is
   accepted: one group per sign, groups going from biggest to
   smallest, each group 1 to 9 signs.  A byte that isn't a sign, a
   sign cut short, groups out of order, or a sign used in two places
   is PN_ERROR_SYNTAX.  Ten of one sign is PN_ERROR_RANGE instead: a
   scribe would have written one sign of the next power, so that
   group is a value too big for its place, not a spelling mistake.
   pn_babylonian_parse_from treats six tens-wedges the same way. */
uint64_t* _Nullable pn_egyptian_parse(const char* _Nonnull buf, size_t buflen,
                                      uint64_t* _Nonnull out,
                                      pn_error* _Nullable* _Nullable error) {
    assert(buf && out);

    uint64_t sum = 0; /* built here: *out stays untouched unless all works */
    int prev = -1;    /* which sign the previous run used; runs step down */
    const char *p = buf, *end = buf + buflen;

    while (p < end) {
        int found = -1;

        for (int i = 0; i < N_SIGNS; i++) {
            if (p + SIGN_BYTES <= end && memcmp(p, signs[i], SIGN_BYTES) == 0) {
                found = i;
                break;
            }
        }
        if (found < 0) {
            /*  a byte that starts no sign, or a sign with bytes missing
                            at the end
                        */
            size_t room = (size_t) (end - p);

            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "byte at position %zu of %zu does not"
                                       " start a hieroglyphic sign (%zu bytes"
                                       " left there: not enough)"
                                       "%s",
                                       (size_t) (p - buf), buflen, room,
                                       room < SIGN_BYTES ? " - a sign is 4"
                                                           " bytes whole"
                                                         : "");
            }
            return NULL;
        }
        if (prev >= 0 && found <= prev) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "the sign at position %zu does not step"
                                       " down in power: runs must go from the"
                                       " Heh toward the stroke, and one sign"
                                       " gets a single run",
                                       (size_t) (p - buf));
            }
            return NULL;
        }

        size_t count = 0;
        do {
            p += SIGN_BYTES;
            if (++count > 9u) {
                if (error) {
                    *error = pn_error_init(PN_ERROR_RANGE,
                                           "ten copies of one sign at"
                                           " position %zu: ten strokes are"
                                           " written as one hobble, scribe"
                                           " - a run holds 1 to 9",
                                           (size_t) (p - buf) - SIGN_BYTES);
                }
                return NULL;
            }
        } while (p + SIGN_BYTES <= end && memcmp(p, signs[found], SIGN_BYTES) == 0);

        /*  Can't overflow: with at most 9 of each of the 7 signs the
                    total is at most 9 999 999.
                */
        sum += count * values[found];
        prev = found;
    }

    /* An empty string is a blank: it reads as 0. */
    *out = sum;
    return out;
}
