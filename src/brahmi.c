/*
    brahmi.c — Brahmi numerals in UTF-8, from 0 to 9999.

    Brahmi numerals were used in India from about the 3rd century
    BCE, and the familiar digits 1 to 9 descend from them. They were
    not yet positional. Instead, every value 1 to 9 and every ten 10
    to 90 has its own sign, so 47 is two signs (forty, seven), not
    eleven marks as in Egyptian:

    𑁒 .. 𑁚  U+11052..U+1105A BRAHMI NUMBER ONE..NINE      = 1..9
    𑁛 .. 𑁣  U+1105B..U+11063 BRAHMI NUMBER TEN..NINETY    = 10..90
    𑁤       U+11064          BRAHMI NUMBER ONE HUNDRED    = 100
    𑁥       U+11065          BRAHMI NUMBER ONE THOUSAND   = 1 000
    𑁿       U+1107F          BRAHMI NUMBER JOINER

    47 is 𑁞𑁘 (forty, seven).

    Hundreds and thousands use multiplication. For 200 to 900 the
    hundred sign is joined to a digit: HUNDRED, JOINER, FOUR means
    400, drawn as one combined sign. The joiner is what shows
    multiplication: HUNDRED, FOUR with no joiner means 100 + 4 = 104.
    A plain 100 or 1000 is just the power sign alone. So:

    400  = 𑁤𑁿𑁕          HUNDRED JOINER FOUR
    104  = 𑁤𑁕            HUNDRED FOUR
    2222 = 𑁥𑁿𑁓𑁤𑁿𑁓𑁜𑁓      THOUSAND JOINER TWO, HUNDRED JOINER TWO,
                         TWENTY, TWO

    There is no zero and no sign above one thousand, so 0 is the
    empty string and the largest value is 9999.

    Reading works slot by slot: thousands, hundreds, tens, ones, each
    used at most once and in that order. 2222 writes TWO three
    times, which is fine because each TWO belongs to a different
    slot. Rejected: two power signs in a row, a joiner with no
    digit after it, HUNDRED FOUR FOUR (the ones slot used twice), and
    slots out of order.

    The code spells the signs as \U escapes so the exact code point
    is visible; the comments show the signs themselves.

    Further reading:
    - https://en.wikipedia.org/wiki/Brahmi_numerals
    - https://www.unicode.org/charts/PDF/U11000.pdf (the Unicode chart)
*/
#include <assert.h>
#include <string.h>

#include "pantheon.h"

#define ONE        "\U00011052" /* = 1      UTF-8: F0 91 81 92 */
#define TWO        "\U00011053" /* = 2      UTF-8: F0 91 81 93 */
#define THREE      "\U00011054" /* = 3      UTF-8: F0 91 81 94 */
#define FOUR       "\U00011055" /* = 4      UTF-8: F0 91 81 95 */
#define FIVE       "\U00011056" /* = 5      UTF-8: F0 91 81 96 */
#define SIX        "\U00011057" /* = 6      UTF-8: F0 91 81 97 */
#define SEVEN      "\U00011058" /* = 7      UTF-8: F0 91 81 98 */
#define EIGHT      "\U00011059" /* = 8      UTF-8: F0 91 81 99 */
#define NINE       "\U0001105A" /* = 9      UTF-8: F0 91 81 9A */
#define TEN        "\U0001105B" /* = 10     UTF-8: F0 91 81 9B */
#define TWENTY     "\U0001105C" /* = 20     UTF-8: F0 91 81 9C */
#define THIRTY     "\U0001105D" /* = 30     UTF-8: F0 91 81 9D */
#define FORTY      "\U0001105E" /* = 40     UTF-8: F0 91 81 9E */
#define FIFTY      "\U0001105F" /* = 50     UTF-8: F0 91 81 9F */
#define SIXTY      "\U00011060" /* = 60     UTF-8: F0 91 81 A0 */
#define SEVENTY    "\U00011061" /* = 70     UTF-8: F0 91 81 A1 */
#define EIGHTY     "\U00011062" /* = 80     UTF-8: F0 91 81 A2 */
#define NINETY     "\U00011063" /* = 90     UTF-8: F0 91 81 A3 */
#define HUNDRED    "\U00011064" /* = 100    UTF-8: F0 91 81 A4 */
#define THOUSAND   "\U00011065" /* = 1 000  UTF-8: F0 91 81 A5 */
#define JOINER     "\U0001107F" /* the joiner, which marks multiplication */
#define SIGN_BYTES 4            /* every sign is 4 bytes in UTF-8 */

static const char* const units[9] = {ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE};

static const char* const tens[9] = {TEN,   TWENTY,  THIRTY, FORTY, FIFTY,
                                    SIXTY, SEVENTY, EIGHTY, NINETY};

/*
    Write n in the standard form described above. buf may be NULL,
    which only counts the bytes. Above PN_BRAHMI_MAX_VALUE the count
    leaves out the digits above the thousands and means nothing;
    pn_brahmi_render reports the error.
*/
static size_t brahmi_emit(uint64_t n, char* buf) {
    size_t bytes = 0;

#define PUT(s)                                    \
    do {                                          \
        if (buf != NULL)                          \
            memcpy(buf + bytes, (s), SIGN_BYTES); \
        bytes += SIGN_BYTES;                      \
    } while (0)

    /*
        One hundreds or thousands term: the power sign alone for a 1,
        POWER JOINER DIGIT for 2 to 9, and nothing for 0.
    */
#define PUT_POWER(power, d)       \
    do {                          \
        if ((d) >= 1u)            \
            PUT(power);           \
        if ((d) >= 2u) {          \
            PUT(JOINER);          \
            PUT(units[(d) - 1u]); \
        }                         \
    } while (0)

    const unsigned d4 = (unsigned) ((n / 1000u) % 10u);
    const unsigned d3 = (unsigned) ((n / 100u) % 10u);
    const unsigned d2 = (unsigned) ((n / 10u) % 10u);
    const unsigned d1 = (unsigned) (n % 10u);

    PUT_POWER(THOUSAND, d4);
    PUT_POWER(HUNDRED, d3);
    if (d2 != 0u) {
        PUT(tens[d2 - 1u]);
    }
    if (d1 != 0u) {
        PUT(units[d1 - 1u]);
    }
#undef PUT_POWER
#undef PUT
    return bytes;
}

size_t pn_brahmi_render_size(uint64_t n) {
    /*
        Never fails: it counts exactly what brahmi_emit writes, which is
        what pn_brahmi_render writes.
    */
    return brahmi_emit(n, NULL);
}

char* _Nullable pn_brahmi_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                                 pn_error* _Nullable* _Nullable error) {
    assert(buf);
    if (n > PN_BRAHMI_MAX_VALUE) {
        if (error) {
            *error =
                pn_error_init(PN_ERROR_RANGE,
                              "%llu runs past %llu: no sign"
                              " above the thousand exists"
                              " (ten thousand is words,"
                              " not signs)",
                              (unsigned long long) n, (unsigned long long) PN_BRAHMI_MAX_VALUE);
        }
        return NULL;
    }

    const size_t need = brahmi_emit(n, NULL); /* exact, so buflen must also hold the 0 byte */

    if (buflen <= need) {
        if (error) {
            *error = pn_error_init(PN_ERROR_BUFFER,
                                   "buffer holds %zu bytes; this"
                                   " string needs %zu plus the"
                                   " terminating NUL",
                                   buflen, need);
        }
        return NULL;
    }
    (void) brahmi_emit(n, buf);
    buf[need] = '\0'; /* n == 0 writes an empty string */
    return buf;
}

/*
    Reading is the reverse of writing, and only the standard form is
    accepted, so reading back written text always gives the original
    number. The text is at most four terms (thousands, hundreds,
    tens, ones), each at most once, in that order, with nothing
    between them. A hundreds or thousands term is the power sign
    alone (for 1) or POWER JOINER DIGIT (for 2 to 9). A tens or ones
    term is its single sign.

    Rejected: bytes that aren't Brahmi numerals, a sign cut off in
    the middle, a term out of order, a joiner anywhere else, HUNDRED
    JOINER ONE (100 is written as the hundred sign alone), and
    HUNDRED FOUR FOUR.
*/

/*
    All the signs in Unicode order: ones, tens, the two powers, then
    the joiner. A term never starts with the joiner.
*/
#define N_SIGNS 21
static const char* const signs[N_SIGNS] = {
    ONE,    TWO,   THREE, FOUR,  FIVE,    SIX,    SEVEN,  EIGHT,   NINE,     TEN,   TWENTY,
    THIRTY, FORTY, FIFTY, SIXTY, SEVENTY, EIGHTY, NINETY, HUNDRED, THOUSAND, JOINER};
#define SIG_NINE     8
#define SIG_TEN      9
#define SIG_NINETY   17
#define SIG_HUNDRED  18
#define SIG_THOUSAND 19
#define SIG_JOINER   20

/*
    Read one whole sign at p: returns its index in the table above, or
    -1 for a byte that isn't a sign or a sign cut off short.
*/
static int match_sign(const char* p, const char* end) {
    for (int i = 0; i < N_SIGNS; i++) {
        if (p + SIGN_BYTES <= end && memcmp(p, signs[i], SIGN_BYTES) == 0) {
            return i;
        }
    }
    return -1;
}

uint64_t* _Nullable pn_brahmi_parse(const char* _Nonnull buf, size_t buflen, uint64_t* _Nonnull out,
                                    pn_error* _Nullable* _Nullable error) {
    assert(buf && out);
    if (buflen == 0) {
        *out = 0; /* an empty string reads as 0 */
        return out;
    }

    uint64_t sum = 0; /* built here; *out changes only if the whole parse succeeds */
    int slot = 0;     /* the next slot allowed: 0 thousands, 1 hundreds, 2 tens, 3 ones */
    const char *p = buf, *end = buf + buflen;

    while (p < end) {
        const int s = match_sign(p, end);

        if (s < 0) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "position %zu holds a foreign"
                                       " byte or a sign cut off short",
                                       (size_t) (p - buf));
            }
            return NULL;
        }
        if (s == SIG_JOINER) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "a joiner at position %zu with"
                                       " nothing being joined to it"
                                       " (doubled, leading, or off its"
                                       " power sign)",
                                       (size_t) (p - buf));
            }
            return NULL;
        }
        p += SIGN_BYTES;

        uint64_t worth;
        int next; /* the next term must be in a later slot */

        if (s == SIG_THOUSAND || s == SIG_HUNDRED) {
            const int is_thousand = (s == SIG_THOUSAND);
            const uint64_t power = is_thousand ? 1000u : 100u;

            if (slot > (is_thousand ? 0 : 1)) {
                if (error) {
                    *error = pn_error_init(PN_ERROR_SYNTAX, "a second term of this"
                                                            " power, or an ascending"
                                                            " one: the pillar walks"
                                                            " down from the thousand");
                }
                return NULL;
            }

            /*
                The digit: the power sign alone means 1; JOINER DIGIT
                multiplies it.
            */
            worth = power;
            if (p + SIGN_BYTES <= end && memcmp(p, JOINER, SIGN_BYTES) == 0) {
                p += SIGN_BYTES;
                const int c = match_sign(p, end);

                if (c < 0 || c > SIG_NINE) {
                    if (error) {
                        *error = pn_error_init(PN_ERROR_SYNTAX,
                                               "the joiner at position"
                                               " %zu joins to a"
                                               " non-multiplier (a tens"
                                               " sign, a power, a second"
                                               " joiner) or to nothing",
                                               (size_t) (p - buf) - SIGN_BYTES);
                    }
                    return NULL;
                }
                if (c == 0) {
                    if (error) {
                        *error = pn_error_init(PN_ERROR_SYNTAX, "1 times the power: write"
                                                                " the power sign bare,"
                                                                " not ligatured");
                    }
                    return NULL;
                }
                worth = (uint64_t) (c + 1) * power;
                p += SIGN_BYTES;
            }
            next = is_thousand ? 1 : 2;
        }
        else if (s >= SIG_TEN && s <= SIG_NINETY) {
            if (slot > 2) {
                if (error) {
                    *error = pn_error_init(PN_ERROR_SYNTAX, "a tens term twice, or after"
                                                            " the ones term");
                }
                return NULL;
            }
            worth = (uint64_t) (s - SIG_TEN + 1) * 10u;
            next = 3;
        }
        else { /* a ones sign: the ones term */
            if (slot > 3) {
                if (error) {
                    *error = pn_error_init(PN_ERROR_SYNTAX, "the ones term is already"
                                                            " spoken");
                }
                return NULL;
            }
            worth = (uint64_t) (s + 1);
            next = 4;
        }

        /*
            Can't overflow: all four slots at most is
            9000 + 900 + 90 + 9 = PN_BRAHMI_MAX_VALUE.
        */
        sum += worth;
        slot = next;
    }
    *out = sum;
    return out;
}
