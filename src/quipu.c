/*
    quipu.c — Inca quipu (knotted cord) numerals in UTF-8.

    The Inca kept records on quipus: a main cord with many hanging
    cords, each one holding a number as clusters of knots. The
    clusters are decimal places, with the ones place nearest the
    loose end of the cord and higher powers of ten closer to the main
    cord. Leland Locke worked this out in 1912.

    Two kinds of knot are used:
    - In the ones place, a "long knot" with d turns means d. A long
      knot can't have just one turn, so 1 is a figure-eight knot.
    - In every other place, d simple knots mean d.
    So the knot type also tells you which cluster is the ones place.

    Unicode has no quipu signs, so this module borrows two common
    ones (3 bytes each in UTF-8):

    •  U+2022 BULLET    E2 80 A2    a simple knot (other places)
    ∞  U+221E INFINITY  E2 88 9E    one turn of a long knot, or the
                                   figure-eight (ones place)

    Places are written biggest first with one space between
    neighbouring places, the same as rods.c. A zero digit is just no
    knots, so the number of spaces is always (number of places - 1):

    23   = ••␣∞∞∞
    101  = •␣␣∞      (empty tens place)
    10   = •␣        (empty ones place)
    1    = ∞         (the figure-eight)

    (␣ marks a space.)

    The signs come from the PN_QUIPU_* constants in pantheon.h.

    Further reading:
    - https://en.wikipedia.org/wiki/Quipu
    - Locke, The Ancient Quipu or Peruvian Knot Record (1912)
    - Ascher and Ascher, Mathematics of the Incas: Code of the Quipu (1997)
*/
#include <assert.h>
#include <string.h>

#include "pantheon.h"

#define SIMPLE_PREFIX "\u2022" /* the bullet:   E2 80 A2         */
#define FIGURE_PREFIX "\u221e" /* infinity:     E2 88 9E         */
#define SIGN_BYTES    3        /* both signs are 3 bytes in UTF-8 */

/*
    The powers of ten from 10^0 to 10^(PN_QUIPU_MAX_PLACES - 1): the
    value of a knot in each place.
*/
static const uint64_t pow10[PN_QUIPU_MAX_PLACES] = {1ull,
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

/*
    Which knot is at p (at least SIGN_BYTES bytes must remain before
    end): 0 for a simple knot, 1 for a long-knot turn, -1 for
    anything else. Look-alike characters (U+2023 TRIANGULAR BULLET,
    U+221D PROPORTIONAL TO, U+00B7 MIDDLE DOT) and a sign cut off in
    the middle all return -1.
*/
static int quipu_match_knot(const char* p, const char* end) {
    if (p + SIGN_BYTES > end) {
        return -1;
    }
    if (memcmp(p, SIMPLE_PREFIX, SIGN_BYTES) == 0) {
        return 0;
    }
    if (memcmp(p, FIGURE_PREFIX, SIGN_BYTES) == 0) {
        return 1;
    }
    return -1;
}

/*
    Write n's knots, or only count the bytes when buf is NULL. The
    count leaves out the 0 byte at the end. It handles one place
    more than the library allows, so for n above PN_QUIPU_MAX_VALUE it
    returns a number that means nothing instead of crashing;
    pn_quipu_render reports PN_ERROR_RANGE before writing anything.
*/
static size_t quipu_emit(uint64_t n, char* buf) {
    uint8_t dig[PN_QUIPU_MAX_PLACES + 2];
    int m = 0;

    do { /* smallest place first */
        dig[m++] = (uint8_t) (n % 10u);
        n /= 10u;
    } while (n != 0);

    size_t bytes = 0;

    for (int i = m - 1; i >= 0; i--) { /* the biggest place */
        if (i != m - 1) {
            if (buf != NULL) {
                buf[bytes] = ' '; /* one space between neighbouring places */
            }
            bytes += 1u;
        }
        const char* sign = (i == 0) ? FIGURE_PREFIX : SIMPLE_PREFIX;

        for (size_t k = 0; k < (size_t) dig[i]; k++) { /* whole signs only */
            if (buf != NULL) {
                memcpy(buf + bytes + k * SIGN_BYTES, sign, SIGN_BYTES);
            }
        }
        bytes += (size_t) dig[i] * SIGN_BYTES; /* a zero place adds no knots */
    }
    return bytes;
}

size_t pn_quipu_render_size(uint64_t n) {
    /*
        Never fails: it counts exactly what quipu_emit writes. Above
        PN_QUIPU_MAX_VALUE the number means nothing, and
        pn_quipu_render reports PN_ERROR_RANGE.
    */
    return quipu_emit(n, NULL);
}

char* _Nullable pn_quipu_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                                pn_error* _Nullable* _Nullable error) {
    assert(buf);
    if (n > PN_QUIPU_MAX_VALUE) {
        if (error) {
            *error = pn_error_init(PN_ERROR_RANGE,
                                   "%llu needs a twentieth line: 10^19"
                                   " no longer fits the cord",
                                   (unsigned long long) n);
        }
        return NULL;
    }

    const size_t need = quipu_emit(n, NULL); /* exact, so buflen must also hold the 0 byte */

    if (buflen <= need) {
        if (error) {
            *error = pn_error_init(PN_ERROR_BUFFER,
                                   "buffer holds %zu bytes; this cord"
                                   " needs %zu plus the terminating"
                                   " NUL",
                                   buflen, need);
        }
        return NULL;
    }
    (void) quipu_emit(n, buf);
    buf[need] = '\0';
    return buf;
}

/*
    Reading is the reverse of writing. The number of places is the
    number of spaces + 1, the same rule as rods.c. Each place's knot
    type is fixed by its position (long knots only in the ones place,
    simple knots everywhere else), and a cluster of knots must end
    exactly where the next space begins.

    Ten or more knots in one cluster is PN_ERROR_RANGE: a digit only
    goes up to 9, the same way babylonian.c treats too many wedges.
    More places than a uint64_t can hold is PN_ERROR_RANGE too.
*/
uint64_t* _Nullable pn_quipu_parse(const char* _Nonnull buf, size_t buflen, uint64_t* _Nonnull out,
                                   pn_error* _Nullable* _Nullable error) {
    assert(buf && out);
    if (buflen == 0) {
        *out = 0; /* an empty string reads as 0      */
        return out;
    }
    if (buf[0] == ' ') {
        if (error) {
            *error = pn_error_init(PN_ERROR_SYNTAX, "the cord never starts inside a"
                                                    " gutter");
        }
        return NULL;
    }

    size_t spaces = 0; /* counted first: it gives the number of places */

    for (size_t i = 0; i < buflen; i++) {
        if (buf[i] == ' ') {
            spaces++;
        }
    }
    if (spaces + 1u > (size_t) PN_QUIPU_MAX_PLACES) {
        if (error) {
            *error = pn_error_init(PN_ERROR_RANGE,
                                   "%zu space(s) ask for %zu places:"
                                   " more cord than a uint64_t holds",
                                   spaces, spaces + 1u);
        }
        return NULL;
    }

    uint64_t sum = 0;         /* *out changes only on success      */
    int place = (int) spaces; /* the current place; each space steps down one */
    int col_kind = -1;        /* the knot cluster being read           */
    int col_count = 0;
    int col_place = -1; /* the place where this cluster started  */
    const char* p = buf;
    const char* end = buf + buflen;

    while (p < end) {
        if (*p == ' ') {
            if (col_count != 0) {      /* a space ends this cluster */
                if (col_place != place /* a space inside the cluster, or */
                    || (col_kind == 1) != (place == 0)) {
                    if (error) {
                        *error =
                            pn_error_init(PN_ERROR_SYNTAX,
                                          "%d knot(s) of the %s family sit"
                                          " in the wrong line (the %s line"
                                          " alone takes them)",
                                          col_count, (col_kind == 1) ? "figure-eight" : "simple",
                                          (place == 0) ? "units" : "upper");
                    }
                    return NULL;
                }
                sum += (uint64_t) col_count * pow10[place];
                col_count = 0;
                col_kind = -1;
                col_place = -1;
            }
            place--;
            p++;
            continue;
        }
        const int kind = quipu_match_knot(p, end);

        if (kind < 0) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "position %zu of the cord holds a"
                                       " foreign byte or half a knot",
                                       (size_t) (p - buf));
            }
            return NULL;
        }
        if (col_count == 0) { /* a cluster starts here */
            if (place < 0) {
                if (error) {
                    *error = pn_error_init(PN_ERROR_SYNTAX, "knots dangling past the free"
                                                            " end of the cord");
                }
                return NULL;
            }
            col_kind = kind;
            col_place = place;
        }
        else if (kind != col_kind) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "one column never mixes the two"
                                       " knot families - position %zu"
                                       " switches families",
                                       (size_t) (p - buf));
            }
            return NULL;
        }
        if (col_count == 9) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE, "a tenth turn in this column:"
                                                       " a digit holds at most 9");
            }
            return NULL;
        }
        col_count++;
        p += SIGN_BYTES;
    }
    if (col_count != 0) { /* the ones-place cluster ends */
        if ((col_kind == 1) != (place == 0)) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "%d knot(s) of the %s family sit"
                                       " in the wrong line",
                                       col_count, (col_kind == 1) ? "figure-eight" : "simple");
            }
            return NULL;
        }
        sum += (uint64_t) col_count * pow10[place];
    }
    /*
        No final check is needed on where the walk ended. A string
        may end with an empty place (10 is a bullet and a space,
        with nothing in the ones place), and since the number of
        places was already fixed by counting spaces, the walk
        can only end in the right spot. Each cluster has
        already checked that it ended at its own place.
    */
    *out = sum;
    return out;
}
