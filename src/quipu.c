/*  quipu.c — libPantheon: Inca quipu knot notation, UTF-8.

    Positional decimal strung along a pendant cord: the units line
    sits nearest the free end, and the powers of ten march toward
    the main cord (the quipucamayuq read bottom-up; this
    transcription lays the cord down most-significant-first, the way
    rods.c linearises the counting board).  Unicode ships no quipu
    signs - no knot block to borrow, unlike cuneiform's wedges in
    babylonian.c - so two ordinary BMP glyphs do the transcribing
    (three UTF-8 bytes each):

    U+2022 BULLET    E2 80 A2    d of them = d simple knots (Locke's
                                "d s") in every line above the units
    U+221E INFINITY  E2 88 9E    d of them = the long knot of d turns
                                at the units line ("d L"); a lone one
                                is the figure-eight (Locke's "E") -
                                a long knot of one turn cannot be
                                tied

    The units line is drawn in the figure-eight family ONLY, and
    every line above it in simple knots ONLY: Locke's distinctive
    units knot becomes this grammar's form-must-match-place rule, the
    knot answer to the rods' alternating sets.  Zero is the ABSENCE
    of a knot in the right spot - no sign for it exists and none is
    invented: a blank place ties no knots, every place boundary
    writes exactly one space, and a well-formed cord holds exactly
    (places - 1) spaces.  From that the parser recoversthe geometry unaided:
    places - 1 spaces means places - 1 place steps, so every column
    ends its run exactly where the next boundary begins.  Ten
    identical knots is more turns than any digit holds (range, the
    same ruling the tenth DISH falls under at babylonian.c); a
    twentieth column is more cord than a uint64_t can hold.  The
    knots arrive as universal character names through the header's
    PN_QUIPU_* signs, which keeps this source pure ASCII.

    References: https://en.wikipedia.org/wiki/Quipu ,
    Locke, The Ancient Quipu or Peruvian Knot Record (1912);
    Ascher & Ascher, Mathematics of the Incas: Code of the Quipu
    (1997).
*/
#include <assert.h>
#include <string.h>

#include "pantheon.h"

#define SIMPLE_PREFIX "\u2022" /* whole bullet:   E2 80 A2        */
#define FIGURE_PREFIX "\u221e" /* whole infinity: E2 88 9E        */
#define SIGN_BYTES    3        /* both signs are BMP: 3 UTF-8 bytes */

/* 10^0 through 10^(PN_QUIPU_MAX_PLACES - 1): the weight of every
   line a pendant holds, on a cord a uint64_t can carry. */
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

/* Which knot sits at p, where at least SIGN_BYTES of room remains
   before end: 0 for the simple knot, 1 for the figure-eight family,
   -1 for anything else.  Whole knots or nothing: near misses
   (U+2023 TOP BULLET, U+221D PROPORTIONAL TO, U+00B7 MIDDLE DOT, the
   Mayan, Brahmi and rod glyphs) all fall through, and so does a knot
   cut off in the middle of its UTF-8 - the room check says its bytes
   are no knot. */
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

/* The shared emit-and-count helper: buf == NULL counts bytes,
   otherwise it writes them - the count never includes the
   terminating NUL.   +1 place: the count keeps going one digit past
   the domain, giving answers above PN_QUIPU_MAX_VALUE that are
   meaningless but never fatal - render turns down such n with
   PN_ERROR_RANGE before it ever writes. */
static size_t quipu_emit(uint64_t n, char* buf) {
    uint8_t dig[PN_QUIPU_MAX_PLACES + 2];
    int m = 0;

    do { /*least significant place first */
        dig[m++] = (uint8_t) (n % 10u);
        n /= 10u;
    } while (n != 0);

    size_t bytes = 0;

    for (int i = m - 1; i >= 0; i--) { /*top of the pendant */
        if (i != m - 1) {
            if (buf != NULL) {
                buf[bytes] = ' '; /*one boundary per place step */
            }
            bytes += 1u;
        }
        const char* sign = (i == 0) ? FIGURE_PREFIX : SIMPLE_PREFIX;

        for (size_t k = 0; k < (size_t) dig[i]; k++) { /*whole knots only */
            if (buf != NULL) {
                memcpy(buf + bytes + k * SIGN_BYTES, sign, SIGN_BYTES);
            }
        }
        bytes += (size_t) dig[i] * SIGN_BYTES; /*a blank place adds none */
    }
    return bytes;
}

size_t pn_quipu_render_size(uint64_t n) {
    /*  This answer never fails: it is exactly what quipu_emit writes.
  Past PN_QUIPU_MAX_VALUE the count runs on and the answer means
  nothing - render turns down such n with a PN_ERROR_RANGE note
  anyway.
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

    const size_t need = quipu_emit(n, NULL); /*exact: buflen must hold +NUL */

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

/* Parsing reads the cord back.

   The whole cord is knots and boundary spaces in canonical spacing.
   The space count pins the geometry (places = spaces + 1, the rods'
   law), and the column's OWN place decides which knot family is
   legal in it - captured the moment the column opens, so trailing
   boundaries can never frame an innocent knot.  A column must also
   END exactly where its last boundary begins: that check is what
   makes "well-formed" mean something.  An interior gutter would end
   a column a place too early, and the topmost column may not sit
   under a gutter either, since the first space always steps down.
   Ten of any knot overfills a digit long before the cord can
   overfill a uint64_t: the babylonian tenth-DISH ruling moved from
   syntax to range on the knot count, while the twentieth place stays
   the page-overrun range call. */
uint64_t* _Nullable pn_quipu_parse(const char* _Nonnull buf, size_t buflen, uint64_t* _Nonnull out,
                                   pn_error* _Nullable* _Nullable error) {
    assert(buf && out);
    if (buflen == 0) {
        *out = 0; /*the empty cord: no knot was tied   */
        return out;
    }
    if (buf[0] == ' ') {
        if (error) {
            *error = pn_error_init(PN_ERROR_SYNTAX, "the cord never starts inside a"
                                                    " gutter");
        }
        return NULL;
    }

    size_t spaces = 0; /*counted first: it defines the places */

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

    uint64_t sum = 0;         /**out keeps its old value unless all works */
    int place = (int) spaces; /*topmost line; steps down each gutter     */
    int col_kind = -1;        /*the column being tied                    */
    int col_count = 0;
    int col_place = -1; /*where it OPENED - the anchor             */
    const char* p = buf;
    const char* end = buf + buflen;

    while (p < end) {
        if (*p == ' ') {
            if (col_count != 0) {      /*a boundary ends this column */
                if (col_place != place /*gutter inside the run, or   */
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
        if (col_count == 0) { /*the column opens right here */
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
    if (col_count != 0) { /*the units column closes */
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
    /*  No "walk ended at place -1" demand copied from muscle memory
  of a full-cord parser: a cord may END on a blank place (10
  renders one bullet then a space - the units line stays
  empty), and with places = spaces + 1 already enforced up
  front, the walk cannot land anywhere else.  The one
  structural demand left is made by the columns themselves:
  col_place must equal the place they close at.
*/
    *out = sum;
    return out;
}
