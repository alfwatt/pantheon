/*  rods.c — libPantheon: counting rod numerals, UTF-8.

    True positional decimal on the counting board: one bundle of rods
    per column, with the columns alternating between the two forms of
    the digits so a lone rod can never be mixed up with its rotated
    twin.  The Unicode block carries both forms (SMP, 4 UTF-8 bytes
    each; all of them share the stem F0 9D 8D, and the last byte runs
    A0..B1):

    U+1D360..U+1D368  COUNTING ROD UNIT DIGIT ONE..NINE
    U+1D369..U+1D371  COUNTING ROD TENS DIGIT ONE..NINE

    The units column takes the unit set, the tens takes the tens set,
    the hundreds take the unit set again - the Song dynasty
    orientation, and what the Unicode names assume.

    The rods had no zero: an empty column stayed literally empty and
    the reader counted columns.  We keep the blank as a space, the
    cuneiform gap grown to base ten - every place boundary writes
    exactly one space and a blank place writes one of its own, so a
    well-formed string holds exactly (places - 1) spaces:

    rod-nine rod-three   = 23
    rod-one  [blank] rod-one = 101     (the blank tens column)
    rod-one  [blank][blank]   = 100    (trailing blanks are spaces too)

    That law is what lets the blank survive the round trip: the
    parser sets places = spaces + 1, reads the leftmost glyph at
    place places - 1, and every glyph's form must match its place's
    parity - the unit-set rod for 11 where the tens-set one belongs
    is the rod version of a Babylonian missing-zero.

    The signs are spelled as universal character names, which keeps
    this source pure ASCII.

    Reference: https://en.wikipedia.org/wiki/Counting_rods
*/
#include <assert.h>
#include <string.h>

#include "pantheon.h"

#define RODS_PREFIX "\xF0\x9D\x8D" /* F0 9D 8D: the shared 3-byte stem of all 18 rods */
#define RODS_LO     0xA0           /* last byte of U+1D360 ROD UNIT ONE   */
#define RODS_MID    0xA9           /* last byte of U+1D369 ROD TENS ONE  */
#define RODS_HI     0xB1           /* last byte of U+1D371 ROD TENS NINE */
#define SIGN_BYTES  4              /* all rods are SMP: 4 UTF-8 bytes    */

/* set 0 = unit digits (even places), set 1 = tens digits (odd places). */
static const char* const signs[2][9] = {
    {"\U0001D360", "\U0001D361", "\U0001D362", "\U0001D363", "\U0001D364", "\U0001D365",
     "\U0001D366", "\U0001D367", "\U0001D368"},
    {"\U0001D369", "\U0001D36A", "\U0001D36B", "\U0001D36C", "\U0001D36D", "\U0001D36E",
     "\U0001D36F", "\U0001D370", "\U0001D371"},
};

static const uint64_t pow10[PN_ROD_MAX_PLACES] = {1ull,
                                                  10ull,
                                                  100ull,
                                                  1000ull,
                                                  10000ull,
                                                  100000ull,
                                                  1000000ull,
                                                  10000000ull,
                                                  100000000ull,
                                                  1000000000ull,
                                                  10000000000ull,
                                                  100000000000ull,
                                                  1000000000000ull,
                                                  10000000000000ull,
                                                  100000000000000ull,
                                                  1000000000000000ull,
                                                  10000000000000000ull,
                                                  100000000000000000ull,
                                                  1000000000000000000ull};

size_t pn_rods_render_size(uint64_t n) {
    /* This answer never fails.  For n within PN_ROD_MAX_VALUE it is
	 *exactly 4 * (count of nonzero digits) + (count of digits - 1):
	  *each place boundary from the top down to the units writes one
	  *space.  A bigger n keeps counting as though the extra columns
	  *could still be drawn, and the answer simply means nothing —
	  *render turns it down with a PN_ERROR_RANGE note anyway. */
    size_t bytes = 0;
    int n_digits = 0;

    while (n != 0) {
        if (n % 10u != 0u) {
            bytes += SIGN_BYTES;
        }
        n_digits++;
        n /= 10u;
    }
    return bytes + (size_t) (n_digits > 0 ? n_digits - 1 : 0);
}

char* _Nullable pn_rods_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                               pn_error* _Nullable* _Nullable error) {
    assert(buf);
    if (n > PN_ROD_MAX_VALUE) {
        if (error) {
            *error = pn_error_init(PN_ERROR_RANGE,
                                   "%llu asks for a twentieth column:"
                                   " 10^19 no longer fits the board",
                                   (unsigned long long) n);
        }
        return NULL;
    }

    const size_t need = pn_rods_render_size(n); /*exact: buflen must hold +NUL */

    if (buflen <= need) {
        if (error) {
            *error = pn_error_init(PN_ERROR_BUFFER,
                                   "buffer holds %zu bytes; this"
                                   " board needs %zu plus the"
                                   " terminating NUL",
                                   buflen, need);
        }
        return NULL;
    }

    uint8_t dig[PN_ROD_MAX_PLACES]; /*dig[i] = the digit at place i */
    int m = 0;

    while (n != 0) {
        dig[m++] = (uint8_t) (n % 10u);
        n /= 10u;
    }

    char* q = buf; /* safe: need is exact and checked against buflen */

    for (int i = m - 1; i >= 0; i--) { /*most significant first */
        if (i != m - 1) {
            *q++ = ' '; /*one space per place boundary */
        }
        if (dig[i] != 0u) {
            const char* g = signs[i & 1][dig[i] - 1u];

            memcpy(q, g, SIGN_BYTES);
            q += SIGN_BYTES;
        }
    }
    *q = '\0'; /* n == 0: the blank board, then the NUL */
    return buf;
}

/* Parsing reads the board back.

   The whole string is rods and spaces in canonical spacing.  The
   blank column - for two thousand years it had no symbol - is a
   space, so the spaces count exactly (places - 1) and the parser
   recovers places = spaces + 1 from it: the leftmost glyph sits at
   place places - 1, each run of r spaces moves the place along by
   r, and each glyph's set must match its place's parity (the
   alternating orientation is how the reader kept count of columns).
   A foreign byte, a sign cut off short, a leading space, two glyphs
   touching, or a digit in the wrong column is malformed; more than
   PN_ROD_MAX_PLACES places is beyond the board, not bad grammar -
   the same ruling the Egyptian scribe hands ten strokes. */

/* Match a rod at @param p, where at least SIGN_BYTES of room remains
   to @param end: the set (0 unit, 1 tens) comes back as the answer
   and the digit 1..9 through @param digit; anything else gives -1. */
static int match_rod(const char* p, const char* end, int* digit) {
    if (p + SIGN_BYTES > end || memcmp(p, RODS_PREFIX, 3) != 0) {
        return -1;
    }
    const unsigned char b = (unsigned char) p[3];

    if (b >= RODS_LO && b <= RODS_LO + 8u) { /*U+1D360..U+1D368 */
        *digit = (int) (b - RODS_LO) + 1;
        return 0;
    }
    if (b >= RODS_MID && b <= RODS_HI) { /*U+1D369..U+1D371 */
        *digit = (int) (b - RODS_MID) + 1;
        return 1;
    }
    return -1;
}

uint64_t* _Nullable pn_rods_parse(const char* _Nonnull buf, size_t buflen, uint64_t* _Nonnull out,
                                  pn_error* _Nullable* _Nullable error) {
    assert(buf && out);
    if (buflen == 0) {
        *out = 0; /* the empty board reads back as 0 */
        return out;
    }
    if (buf[0] == ' ') {
        if (error) {
            *error = pn_error_init(PN_ERROR_SYNTAX, "the board never starts out blank");
        }
        return NULL;
    }

    size_t spaces = 0; /*counted first: it defines the places */

    for (size_t i = 0; i < buflen; i++) {
        if (buf[i] == ' ') {
            spaces++;
        }
    }
    const int m = (int) (spaces + 1u); /*the top place is m - 1 */

    if (m > PN_ROD_MAX_PLACES) {
        if (error) {
            *error = pn_error_init(PN_ERROR_RANGE,
                                   "%zu space(s) ask for %zu places:"
                                   " more board than a uint64_t"
                                   " can hold",
                                   spaces, (size_t) m);
        }
        return NULL;
    }

    uint64_t sum = 0; /*built here; *out keeps its old value unless all works */
    int place = m - 1;
    int glyphs = 0;
    bool after_glyph = false;
    const char *p = buf, *end = buf + buflen;

    while (p < end) {
        if (*p == ' ') {
            place--; /*every space advances one place */
            after_glyph = false;
            p++;
            continue;
        }
        if (after_glyph) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "two columns with no"
                                       " boundary space between"
                                       " them at position %zu",
                                       (size_t) (p - buf));
            }
            return NULL;
        }

        int digit;
        const int set = match_rod(p, end, &digit);

        if (set < 0) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "position %zu holds a"
                                       " foreign byte or a sign"
                                       " cut off short",
                                       (size_t) (p - buf));
            }
            return NULL;
        }
        if (set != (place & 1)) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "a %s-set rod sits in the"
                                       "%s column at position"
                                       " %zu: a rod's set must"
                                       " match its column",
                                       (set == 0) ? "unit" : "tens", (place & 1) ? "tens" : "unit",
                                       (size_t) (p - buf));
            }
            return NULL;
        }

        /* No overflow possible: place < m <= PN_ROD_MAX_PLACES caps
		   *the sum at 9 * (10^18 + ... + 1) = PN_ROD_MAX_VALUE. */
        sum += (uint64_t) digit * pow10[place];
        glyphs++;
        after_glyph = true;
        p += SIGN_BYTES;
    }

    if (glyphs == 0) {
        if (error) {
            *error = pn_error_init(PN_ERROR_SYNTAX, "all spaces: not one rod on the"
                                                    " board");
        }
        return NULL;
    }
    *out = sum;
    return out;
}
