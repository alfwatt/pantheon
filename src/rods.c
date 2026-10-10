/*
    rods.c — Chinese counting rod numerals in UTF-8.

    Chinese mathematicians calculated with small rods laid out in
    columns on a counting board, one column per decimal place. This
    was true place-value decimal, more than a thousand years before
    it reached Europe.

    Each digit 1 to 9 has two forms, one the other turned sideways.
    Columns alternate between the forms, so a reader could tell where
    one column ended and the next began. Unicode has both sets (each
    sign is 4 bytes in UTF-8, all starting F0 9D 8D):

    𝍠 .. 𝍨  U+1D360..U+1D368  COUNTING ROD UNIT DIGIT ONE..NINE
    𝍩 .. 𝍱  U+1D369..U+1D371  COUNTING ROD TENS DIGIT ONE..NINE

    The ones column uses the unit set, the tens column the tens set,
    the hundreds the unit set again, and so on.

    For most of their history the rods had no zero: an empty column
    was just left empty. Here every pair of neighbouring columns is
    separated by one space, and an empty column writes nothing, so
    the number of spaces is always (number of columns - 1):

    𝍪␣𝍢     = 23    (tens-form two, unit-form three)
    𝍠␣␣𝍠    = 101   (empty tens column)
    𝍠␣␣     = 100   (empty tens and ones columns)

    (␣ marks a space.)

    That rule lets an empty column survive being written and read
    back: the reader counts the spaces to find how many columns there
    are, then checks that each rod uses the right form for its
    column.

    The code spells the signs as \U escapes so the exact code point
    is visible; the comments show the signs themselves.

    Further reading:
    - https://en.wikipedia.org/wiki/Counting_rods
*/
#include <assert.h>
#include <string.h>

#include "pantheon.h"

#define RODS_PREFIX "\xF0\x9D\x8D" /* F0 9D 8D: the first 3 bytes of all 18 rods */
#define RODS_LO     0xA0           /* last byte of U+1D360 ROD UNIT ONE   */
#define RODS_MID    0xA9           /* last byte of U+1D369 ROD TENS ONE  */
#define RODS_HI     0xB1           /* last byte of U+1D371 ROD TENS NINE */
#define SIGN_BYTES  4              /* each rod is 4 bytes in UTF-8      */

/* set 0: unit digits (ones, hundreds, ...); set 1: tens digits (tens, thousands, ...). */
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
    /*
        Never fails. For n up to PN_ROD_MAX_VALUE it is exactly
        4 x (number of nonzero digits) + (number of digits - 1): 4 bytes
        per rod, plus one space between neighbouring columns. For a
        bigger n the answer means nothing, and pn_rods_render reports
        PN_ERROR_RANGE.
    */
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

    const size_t need = pn_rods_render_size(n); /* exact, not counting the null terminator */

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

    uint8_t dig[PN_ROD_MAX_PLACES]; /* dig[i] is the digit in column i (0 is the ones) */
    int m = 0;

    while (n != 0) {
        dig[m++] = (uint8_t) (n % 10u);
        n /= 10u;
    }

    char* q = buf; /* safe: the size was checked above */

    for (int i = m - 1; i >= 0; i--) { /* biggest column first */
        if (i != m - 1) {
            *q++ = ' '; /* one space between neighbouring columns */
        }
        if (dig[i] != 0u) {
            const char* g = signs[i & 1][dig[i] - 1u];

            memcpy(q, g, SIGN_BYTES);
            q += SIGN_BYTES;
        }
    }
    *q = '\0'; /* n == 0 writes an empty string */
    return buf;
}

/*
    Reading is the reverse of writing. The text is rods and spaces
    following the spacing rule above, so the number of columns is the
    number of spaces + 1. The first rod is in the top column; each
    space moves one column down. Each rod's form (unit or tens set)
    must match its column.

    Rejected as PN_ERROR_SYNTAX: bytes that aren't rods, a rod cut off
    in the middle, a leading space, two rods with no space between
    them, or a rod in the wrong form for its column. More than
    PN_ROD_MAX_PLACES columns is PN_ERROR_RANGE instead (too big, not
    misspelled), the same way the Egyptian reader treats ten of one
    sign.
*/

/*
    Read a rod at p (at least SIGN_BYTES bytes must remain before
    end). Returns its set (0 unit, 1 tens) and puts the digit 1 to 9
    in *digit, or returns -1 if p holds something else.
*/
static int match_rod(const char* p, const char* end, int* digit) {
    if (p + SIGN_BYTES > end || memcmp(p, RODS_PREFIX, 3) != 0) {
        return -1;
    }
    const unsigned char b = (unsigned char) p[3];

    if (b >= RODS_LO && b <= RODS_LO + 8u) { /* U+1D360 to U+1D368 */
        *digit = (int) (b - RODS_LO) + 1;
        return 0;
    }
    if (b >= RODS_MID && b <= RODS_HI) { /* U+1D369 to U+1D371 */
        *digit = (int) (b - RODS_MID) + 1;
        return 1;
    }
    return -1;
}

uint64_t pn_rods_parse(const char* _Nonnull text, pn_error* _Nullable* _Nullable error) {
    assert(text);

    size_t text_len = strlen(text);
    if (text_len == 0) {
        return 0; /* an empty string reads as 0 */
    }
    if (text[0] == ' ') {
        if (error) {
            *error = pn_error_init(PN_ERROR_SYNTAX, "the board never starts out blank");
        }
        return 0;
    }

    size_t spaces = 0; /* counted first: it gives the number of columns */

    for (size_t i = 0; i < text_len; i++) {
        if (text[i] == ' ') {
            spaces++;
        }
    }
    const int m = (int) (spaces + 1u); /* the top column is m - 1 */

    if (m > PN_ROD_MAX_PLACES) {
        if (error) {
            *error = pn_error_init(PN_ERROR_RANGE,
                                   "%zu space(s) ask for %zu places:"
                                   " more board than a uint64_t"
                                   " can hold",
                                   spaces, (size_t) m);
        }
        return 0;
    }

    uint64_t sum = 0; /* built here; returned only if the whole parse succeeds */
    int place = m - 1;
    int glyphs = 0;
    bool after_glyph = false;
    const char *p = text, *end = text + text_len;

    while (p < end) {
        if (*p == ' ') {
            place--; /* each space moves down one column */
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
                                       (size_t) (p - text));
            }
            return 0;
        }

        int digit;
        const int set = match_rod(p, end, &digit);

        if (set < 0) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "position %zu holds a"
                                       " foreign byte or a sign"
                                       " cut off short",
                                       (size_t) (p - text));
            }
            return 0;
        }
        if (set != (place & 1)) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "a %s-set rod sits in the"
                                       "%s column at position"
                                       " %zu: a rod's set must"
                                       " match its column",
                                       (set == 0) ? "unit" : "tens", (place & 1) ? "tens" : "unit",
                                       (size_t) (p - text));
            }
            return 0;
        }

        /*
            Can't overflow: with at most PN_ROD_MAX_PLACES columns the
            total is at most 9 x (10^18 + ... + 1) = PN_ROD_MAX_VALUE.
        */
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
        return 0;
    }
    return sum;
}
