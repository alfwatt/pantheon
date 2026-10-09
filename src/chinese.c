/*  chinese.c — libPantheon: hanzi numerals, UTF-8.

    Decimal written with multipliers instead of place-value digits:
    the value is A * 10^4 + B, and each section writes its nonzero
    digits from high to low as (digit + 千/百/十), with the section's
    units digit bare and last:

    四千六百二十二 = 4622     (Wikipedia's worked example)
    五千万 五      = 50 000 005

    The blank for zero follows the Egyptian ruling: a skipped digit
    is not written at all, and the skip is admitted by exactly one
    space between the terms on both sides of it - 一千 五 = 1005,
    一千 五十 = 1050, where a later scribe would write 零.  The space
    carries no value (the multipliers pin every digit's place), so it
    is a pure canonical marker and the round trip stays exact.
    Canonical form means the full formal reading: 一十四, never the
    spoken elision 十四.  The domain stops at 万 - no sign above it
    lives here, just as Egypt had none above the Heh - so
    PN_HANZI_MAX_VALUE is 99 999 999.

    The signs are spelled as universal character names, which keeps
    this source pure ASCII.

    Reference: https://en.wikipedia.org/wiki/Chinese_numerals
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
#define SIGN_BYTES 3            /* every sign is BMP: 3 bytes of UTF-8 */

static const char* const digits[9] = {YI, ER, SAN, SI, WU, LIU, QI, BA, JIU};

/* A section's multipliers, largest first: place 3, 2, 1. */
static const char* const mults[3] = {QIAN, BAI, SHI};

/* What a digit at place k (0 = units) inside its section is worth. */
static const uint64_t pow10s[4] = {1u, 10u, 100u, 1000u};

/* Write @param n's canonical hanzi string; @param buf may be NULL,
   which only counts.  Past PN_HANZI_MAX_VALUE the count drops the
   places above 万 and its answer means nothing - render turns down
   such n with a PN_ERROR_RANGE note anyway. */
static size_t hanzi_emit(uint64_t n, char* buf) {
    size_t bytes = 0;
    const uint64_t secs[2] = {n / 10000u, n % 10000u};
    int prev = -1; /* the absolute place of the last term written */

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

            if (d == 0u)
                continue; /* zero: not written, just skipped */
            const int place = base + k;

            if (prev >= 0 && prev - place > 1)
                PUT(" ", 1u); /* the blank: 零's predecessor */
            PUT(digits[d - 1u], SIGN_BYTES);
            if (k > 0)
                PUT(mults[3 - k], SIGN_BYTES);
            prev = place;
        }
        if (s == 0 && sec != 0u)
            PUT(WAN, SIGN_BYTES); /* 万 rides the top section's last term */
    }
#undef PUT
    return bytes;
}

size_t pn_hanzi_render_size(uint64_t n) {
    /* This answer never fails: it is exactly what hanzi_emit writes —
	 *and that is what render will write. */
    return hanzi_emit(n, NULL);
}

char* _Nullable pn_hanzi_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                                pn_error* _Nullable* _Nullable error) {
    assert(buf);
    if (n > PN_HANZI_MAX_VALUE) {
        if (error) *error = pn_error_init(PN_ERROR_RANGE,
                                          "%llu runs past %llu: no sign"
                                          " above \U00004E07 lives in this module"
                                          " (\U00004EBF is not written here)",
                                          (unsigned long long) n, (unsigned long long) PN_HANZI_MAX_VALUE);
        return NULL;
    }

    const size_t need = hanzi_emit(n, NULL); /*exact: buflen must hold +NUL */

    if (buflen <= need) {
        if (error) *error = pn_error_init(PN_ERROR_BUFFER,
                                          "buffer holds %zu bytes; this"
                                          " string needs %zu plus the"
                                          " terminating NUL",
                                          buflen, need);
        return NULL;
    }
    (void) hanzi_emit(n, buf);
    buf[need] = '\0'; /* n == 0 writes just the NUL */
    return buf;
}

/* Parsing reads back what rendering wrote.

   The canonical form is what makes the round trip exact.  A string
   is terms - (digit [千|百|十]) or a bare digit - strictly descending
   in place inside each section, with at most one 万 written right
   after the top section's last term.  Between two terms in a row a
   space shows up if and only if they sit more than one place apart;
   neighboring terms must touch.  Everything else - foreign bytes, a
   sign cut off mid-UTF-8, 十四 missing its 一, 四百 三十一 with an
   unearned space, 五千万五 missing its blank - is refused the way a
   reader refuses "IIIX". */

/* Match a digit sign at @param p: the digit 1..9 comes back, or -1
   for anything else. */
static int match_digit(const char* p, const char* end) {
    for (int i = 0; i < 9; i++)
        if (p + SIGN_BYTES <= end && memcmp(p, digits[i], SIGN_BYTES) == 0)
            return i + 1;
    return -1;
}

/* Match a section multiplier at @param p: 3/2/1 for 千/百/十, 0 for
   none at all.    万 is not a section multiplier; the string is split
   at it first. */
static int match_mult(const char* p, const char* end) {
    if (p + SIGN_BYTES > end)
        return 0;
    if (memcmp(p, QIAN, SIGN_BYTES) == 0)
        return 3;
    if (memcmp(p, BAI, SIGN_BYTES) == 0)
        return 2;
    if (memcmp(p, SHI, SIGN_BYTES) == 0)
        return 1;
    return 0;
}

uint64_t* _Nullable pn_hanzi_parse(const char* _Nonnull buf, size_t buflen, uint64_t* _Nonnull out,
                                   pn_error* _Nullable* _Nullable error) {
    assert(buf && out);
    if (buflen == 0) {
        *out = 0; /* the blank reads back as the nothing it was */
        return out;
    }
    if (buf[0] == ' ' || buf[buflen - 1] == ' ') {
        if (error) *error = pn_error_init(PN_ERROR_SYNTAX,
                                          "the blank sits between terms,"
                                          " never at a border");
        return NULL;
    }

    const char* end = buf + buflen;

    /* Split at 万 first: at most one, written right after the top
	   *section's last term — so never at either border, and no 万 means
	   *no top section at all.    E4 can only ever be a sign's lead byte
	   *(continuations run 80..BF), so a byte scan locates it exactly
	   *where a reader sees it. */
    const char* wan = NULL;

    for (const char* w = buf; w + SIGN_BYTES <= end; w++)
        if (memcmp(w, WAN, SIGN_BYTES) == 0) {
            if (wan != NULL) {
                if (error) *error = pn_error_init(PN_ERROR_SYNTAX,
                                                  "a second \U00004E07 in one string:"
                                                  " there is one \U00004E07 at most");
                return NULL;
            }
            wan = w;
        }
    if (wan != NULL && wan == buf) {
        if (error) *error = pn_error_init(PN_ERROR_SYNTAX,
                                          "\U00004E07 with no top section for it to"
                                          " ride on");
        return NULL;
    }

    const char* const starts[2] = {buf, wan != NULL ? wan + SIGN_BYTES : buf};
    const char* const stops[2] = {wan != NULL ? wan : buf, end};
    const int bases[2] = {4, 0}; /* the 万 section rides four places up */

    uint64_t secs[2] = {0u, 0u}; /* built here; *out keeps its old value unless all works */
    int prev_k[2] = {4, 4};      /* last term's place per section: strict descent */
    int prev = -1;               /* the absolute place of the last term, for spacing */

    for (int s = 0; s < 2; s++) {
        const char* p = starts[s];

        while (p < stops[s]) {
            int run = 0; /* spaces ahead of this term */

            while (p + (size_t) run < stops[s] && p[run] == ' ')
                run++;

            const int d = match_digit(p + run, stops[s]);

            if (d < 0) {
                if (error) *error = pn_error_init(PN_ERROR_SYNTAX,
                                                  "position %zu holds no digit"
                                                  " sign (a foreign byte, a"
                                                  " cut-off sign, or a space"
                                                  " right where a digit"
                                                  " belongs)",
                                                  (size_t) (p - buf));
                return NULL;
            }
            const char* q = p + run + SIGN_BYTES;
            int k = match_mult(q, stops[s]); /* 3/2/1 = the term's place, else bare units */

            if (k >= 1 && k <= 3)
                q += SIGN_BYTES;
            else
                k = 0;
            const int place = bases[s] + k;

            if (k >= prev_k[s]) {
                if (error) *error = pn_error_init(PN_ERROR_SYNTAX,
                                                  "the multiplier at position"
                                                  " %zu climbs or repeats"
                                                  " (or follows a section's"
                                                  " bare units digit): each"
                                                  " section walks its places"
                                                  " downward",
                                                  (size_t) (q - buf - (k > 0 ? SIGN_BYTES : 0)));
                return NULL;
            }
            prev_k[s] = k;
            const int gap = prev - place;

            if (prev < 0) {
                if (run != 0) {
                    if (error) *error = pn_error_init(PN_ERROR_SYNTAX,
                                                      "%d space(s) ahead of"
                                                      " the first term: a"
                                                      " string starts on a"
                                                      " term",
                                                      run);
                    return NULL;
                }
            }
            else if (gap <= 0) {
                if (error) *error = pn_error_init(PN_ERROR_SYNTAX,
                                                  "terms cannot step"
                                                  " backwards in place");
                return NULL;
            }
            else if (gap == 1 && run != 0) {
                if (error) *error = pn_error_init(PN_ERROR_SYNTAX,
                                                  "%d space(s) between terms"
                                                  " only one place apart:"
                                                  " neighbors touch",
                                                  run);
                return NULL;
            }
            else if (gap > 1 && run != 1) {
                if (error) *error = pn_error_init(PN_ERROR_SYNTAX,
                                                  "%d space(s) across a skip"
                                                  " of %d places: a skip is"
                                                  " admitted by exactly one"
                                                  " blank",
                                                  run, gap);
                return NULL;
            }
            secs[s] += (uint64_t) d * pow10s[k];
            prev = place;
            p = q;
        }
    }

    /* The grammar lets in no value past PN_HANZI_MAX_VALUE: each
	   *section tops out at 9999, so no overflow check is needed. */
    *out = secs[0] * 10000u + secs[1];
    return out;
}
