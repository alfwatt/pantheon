/*
    chinese.c — Chinese numerals (hanzi) in UTF-8, from 0 to 99 999 999.

    Chinese numerals are decimal, but instead of using position to
    show each digit's value, they write the digit followed by a
    multiplier sign: 千 (thousand), 百 (hundred), 十 (ten). The ones
    digit has no multiplier. Numbers from 10 000 up are split into
    two sections around 万 (ten thousand): the value is A x 10 000 + B,
    each section written the same way.

    四千六百二十二 = 4622     (4 thousand, 6 hundred, 2 ten, 2)
    五千万 五      = 50 000 005

    A zero digit is simply left out. Because the multipliers say
    where each digit belongs, nothing is lost, but this module marks
    each skip with one space so the reader can see it: 一千 五 = 1005,
    一千 五十 = 1050. (Modern writers use the sign 零 for this.)  The
    space has no value of its own, so reading the text back always
    gives the same number.

    This module uses the full formal form, 一十四 for 14, not the
    everyday short form 十四.  There is no sign above 万 here, so the
    largest value is 99 999 999 (PN_HANZI_MAX_VALUE).

    The code spells the signs as \U escapes so the exact code point
    is visible; the comments show the signs themselves.

    Further reading:
    - https://en.wikipedia.org/wiki/Chinese_numerals
*/
#include <assert.h>
#include <string.h>

#include "pantheon.h"

#define YI         "\U00004E00" /* 一 = 1    UTF-8: E4 B8 80 */
#define ER         "\U00004E8C" /* 二 = 2    UTF-8: E4 BA 8C */
#define SAN        "\U00004E09" /* 三 = 3    UTF-8: E4 B8 89 */
#define SI         "\U000056DB" /* 四 = 4    UTF-8: E5 9B 9B */
#define WU         "\U00004E94" /* 五 = 5    UTF-8: E4 BA 94 */
#define LIU        "\U0000516D" /* 六 = 6    UTF-8: E5 85 AD */
#define QI         "\U00004E03" /* 七 = 7    UTF-8: E4 B8 83 */
#define BA         "\U0000516B" /* 八 = 8    UTF-8: E5 85 AB */
#define JIU        "\U00004E5D" /* 九 = 9    UTF-8: E4 B9 9D */
#define SHI        "\U00005341" /* 十 = 10   UTF-8: E5 8D 81 */
#define BAI        "\U0000767E" /* 百 = 100  UTF-8: E7 99 BE */
#define QIAN       "\U00005343" /* 千 = 1k   UTF-8: E5 8D 83 */
#define WAN        "\U00004E07" /* 万 = 10k  UTF-8: E4 B8 87 */
#define SIGN_BYTES 3            /* every sign is 3 bytes in UTF-8 */

static const char* const digits[9] = {YI, ER, SAN, SI, WU, LIU, QI, BA, JIU};

/* The multipliers within a section, biggest first: places 3, 2, 1. */
static const char* const mults[3] = {QIAN, BAI, SHI};

/* The value of a digit at place k in a section (0 is the ones place). */
static const uint64_t pow10s[4] = {1u, 10u, 100u, 1000u};

/*
    Write n in the standard form described above. buf may be NULL,
    which only counts the bytes. Above PN_HANZI_MAX_VALUE the count
    leaves out the digits above 万 and means nothing; pn_hanzi_render
    reports PN_ERROR_RANGE for those.
*/
static size_t hanzi_emit(uint64_t n, char* buf) {
    size_t bytes = 0;
    const uint64_t secs[2] = {n / 10000u, n % 10000u};
    int prev = -1; /* the place of the last term written, counting from the ones (0) */

#define PUT(s, len)                                   \
    do {                                              \
        if (buf != NULL)                              \
            memcpy(buf + bytes, (s), (size_t) (len)); \
        bytes += (size_t) (len);                      \
    } while (0)

    for (int s = 0; s < 2; s++) {
        const uint64_t sec = secs[s];
        const int base = s == 0 ? 4 : 0;

        for (int k = 3; k >= 0; k--) {
            const unsigned d = (unsigned) ((sec / pow10s[k]) % 10u);

            if (d == 0u) {
                continue; /* a zero digit is not written */
            }
            const int place = base + k;

            if (prev >= 0 && prev - place > 1) {
                PUT(" ", 1u); /* one space marks skipped digits (where modern text writes 零) */
            }
            PUT(digits[d - 1u], SIGN_BYTES);
            if (k > 0) {
                PUT(mults[3 - k], SIGN_BYTES);
            }
            prev = place;
        }
        if (s == 0 && sec != 0u) {
            PUT(WAN, SIGN_BYTES); /* 万 comes right after the top section */
        }
    }
#undef PUT
    return bytes;
}

size_t pn_hanzi_render_size(uint64_t n) {
    /*
        Never fails: it counts exactly what hanzi_emit writes, which is
        what pn_hanzi_render writes.
    */
    return hanzi_emit(n, NULL);
}

char* _Nullable pn_hanzi_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                                pn_error* _Nullable* _Nullable error) {
    assert(buf);
    if (n > PN_HANZI_MAX_VALUE) {
        if (error) {
            *error = pn_error_init(PN_ERROR_RANGE,
                                   "%llu runs past %llu: no sign"
                                   " above \U00004E07 lives in this module"
                                   " (\U00004EBF is not written here)",
                                   (unsigned long long) n, (unsigned long long) PN_HANZI_MAX_VALUE);
        }
        return NULL;
    }

    const size_t need = hanzi_emit(n, NULL); /* exact, so buflen must also hold the 0 byte */

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
    (void) hanzi_emit(n, buf);
    buf[need] = '\0'; /* n == 0 writes only the 0 byte: an empty string */
    return buf;
}

/*
    Reading is the reverse of writing, and only the standard form is
    accepted, so reading back written text always gives the original
    number. The text is a list of terms, each a digit with a
    multiplier (千, 百, 十) or a lone ones digit, going from bigger
    places to smaller. At most one 万 may appear, right after the top
    section. Two terms are separated by a space exactly when digits
    were skipped between them; otherwise they touch.

    Rejected: bytes that aren't numeral signs, a sign cut off in the
    middle, 十四 without its 一, 四百 三十一 (a space where nothing was
    skipped), and 五千万五 (a skip with no space).
*/

/* Read a digit sign at p: returns 1 to 9, or -1 if p holds something else. */
static int match_digit(const char* p, const char* end) {
    for (int i = 0; i < 9; i++) {
        if (p + SIGN_BYTES <= end && memcmp(p, digits[i], SIGN_BYTES) == 0) {
            return i + 1;
        }
    }
    return -1;
}

/*
    Read a multiplier at p: 3 for 千, 2 for 百, 1 for 十, 0 for none.
    万 is not handled here; the text is split at 万 first.
*/
static int match_mult(const char* p, const char* end) {
    if (p + SIGN_BYTES > end) {
        return 0;
    }
    if (memcmp(p, QIAN, SIGN_BYTES) == 0) {
        return 3;
    }
    if (memcmp(p, BAI, SIGN_BYTES) == 0) {
        return 2;
    }
    if (memcmp(p, SHI, SIGN_BYTES) == 0) {
        return 1;
    }
    return 0;
}

uint64_t* _Nullable pn_hanzi_parse(const char* _Nonnull buf, size_t buflen, uint64_t* _Nonnull out,
                                   pn_error* _Nullable* _Nullable error) {
    assert(buf && out);
    if (buflen == 0) {
        *out = 0; /* an empty string reads as 0 */
        return out;
    }
    if (buf[0] == ' ' || buf[buflen - 1] == ' ') {
        if (error) {
            *error = pn_error_init(PN_ERROR_SYNTAX, "the blank sits between terms,"
                                                    " never at a border");
        }
        return NULL;
    }

    const char* end = buf + buflen;

    /*
        Split the text at 万 first. There can be at most one, right
        after the top section, so never at the very start or end; with
        no 万 there is no top section. In UTF-8, the byte E4 can only
        start a sign, never appear in the middle of one (middle bytes
        are 80 to BF), so a simple byte search finds 万 exactly.
    */
    const char* wan = NULL;

    for (const char* w = buf; w + SIGN_BYTES <= end; w++) {
        if (memcmp(w, WAN, SIGN_BYTES) == 0) {
            if (wan != NULL) {
                if (error) {
                    *error = pn_error_init(PN_ERROR_SYNTAX, "a second \U00004E07 in one string:"
                                                            " there is one \U00004E07 at most");
                }
                return NULL;
            }
            wan = w;
        }
    }
    if (wan != NULL && wan == buf) {
        if (error) {
            *error = pn_error_init(PN_ERROR_SYNTAX, "\U00004E07 with no top section for it to"
                                                    " ride on");
        }
        return NULL;
    }

    const char* const starts[2] = {buf, wan != NULL ? wan + SIGN_BYTES : buf};
    const char* const stops[2] = {wan != NULL ? wan : buf, end};
    const int bases[2] = {4, 0}; /* the section before 万 starts four places up */

    uint64_t secs[2] = {0u, 0u}; /* built here; *out changes only if the whole parse succeeds */
    int prev_k[2] = {4, 4};      /* each section's last place, so places must keep going down */
    int prev = -1;               /* the place of the last term, to check the spacing */

    for (int s = 0; s < 2; s++) {
        const char* p = starts[s];

        while (p < stops[s]) {
            int run = 0; /* spaces before this term */

            while (p + (size_t) run < stops[s] && p[run] == ' ') {
                run++;
            }

            const int d = match_digit(p + run, stops[s]);

            if (d < 0) {
                if (error) {
                    *error = pn_error_init(PN_ERROR_SYNTAX,
                                           "position %zu holds no digit"
                                           " sign (a foreign byte, a"
                                           " cut-off sign, or a space"
                                           " right where a digit"
                                           " belongs)",
                                           (size_t) (p - buf));
                }
                return NULL;
            }
            const char* q = p + run + SIGN_BYTES;
            int k =
                match_mult(q, stops[s]); /* 3, 2 or 1 is the term's place; 0 means a ones digit */

            if (k >= 1 && k <= 3) {
                q += SIGN_BYTES;
            }
            else {
                k = 0;
            }
            const int place = bases[s] + k;

            if (k >= prev_k[s]) {
                if (error) {
                    *error = pn_error_init(PN_ERROR_SYNTAX,
                                           "the multiplier at position"
                                           " %zu climbs or repeats"
                                           " (or follows a section's"
                                           " bare units digit): each"
                                           " section walks its places"
                                           " downward",
                                           (size_t) (q - buf - (k > 0 ? SIGN_BYTES : 0)));
                }
                return NULL;
            }
            prev_k[s] = k;
            const int gap = prev - place;

            if (prev < 0) {
                if (run != 0) {
                    if (error) {
                        *error = pn_error_init(PN_ERROR_SYNTAX,
                                               "%d space(s) ahead of"
                                               " the first term: a"
                                               " string starts on a"
                                               " term",
                                               run);
                    }
                    return NULL;
                }
            }
            else if (gap <= 0) {
                if (error) {
                    *error = pn_error_init(PN_ERROR_SYNTAX, "terms cannot step"
                                                            " backwards in place");
                }
                return NULL;
            }
            else if (gap == 1 && run != 0) {
                if (error) {
                    *error = pn_error_init(PN_ERROR_SYNTAX,
                                           "%d space(s) between terms"
                                           " only one place apart:"
                                           " neighbors touch",
                                           run);
                }
                return NULL;
            }
            else if (gap > 1 && run != 1) {
                if (error) {
                    *error = pn_error_init(PN_ERROR_SYNTAX,
                                           "%d space(s) across a skip"
                                           " of %d places: a skip is"
                                           " admitted by exactly one"
                                           " blank",
                                           run, gap);
                }
                return NULL;
            }
            secs[s] += (uint64_t) d * pow10s[k];
            prev = place;
            p = q;
        }
    }

    /*
        No overflow check needed: each section is at most 9999, so the
        total can't pass PN_HANZI_MAX_VALUE.
    */
    *out = secs[0] * 10000u + secs[1];
    return out;
}
