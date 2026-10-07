/*  greek.c — libPantheon: Greek alphabetic (Milesian/Ionic) numerals,
    UTF-8.

    The rule book here is the same one the uint64 notations follow:
    no place-value struct sits between the number and its cord, just
    like the egyptian scribe, the counting rods and the quipu - an
    additive notation has no places to hold.

    The cord is made of: an optional low keraia (U+0375) that
    announces the thousands run, the thousands run itself, ONE
    separating space when both runs are alive, the units run, and
    always the trailing keraia (U+0374).  A run reads its three
    decimal digits as letters: at most one sign from the hundreds row
     (rho..sampi), then one from the tens row (iota..koppa), then one
    from the units row (alpha..theta).  The digit d of a row is that
    row's d-th sign, written ONCE: the market kept its own dedicated
    letter for every one of the 27 digits, so 900 is sampi - not nine
    sampis packed together.  The space kills the butt-joint
    collisions (10001 reads "low-keraia iota, space, alpha" against
    11000's "low-keraia iota alpha"), and it belongs to the canonical
    cord.

    Every sign here is a 2-byte BMP code point: the encoder writes
    canonical 2-byte sequences only, and the decoder lets in no code
    point outside the 27 numeral signs and the two keraia.  Capitals,
    the lunate sigma U+03F2, the final sigma, the digamma U+03DD, the
    tonos, and the NFC twin of the dexia (U+02B9) hold no table entry
    and fall out as syntax errors - which is exactly the culture's
    own verdict on a misspelled number.

    References: https://en.wikipedia.org/wiki/Greek_numerals
    Unicode block Greek and Coptic; keraia at U+0374/U+0375.
*/
#include <string.h>

#include "pantheon.h"

#define GREEK_LOWK  0x375u /* the low keraia: announces the thousands     */
#define GREEK_DEXIA 0x374u /* the trailing keraia: proof of a number      */
#define GREEK_SPACE 0x020u /* the run separator, a canonical byte         */
#define GREEK_MAX_TOKS \
    12u /* low keraia, 3 signs, space, 3 signs, dexia:
                                 nine tokens; a tenth is no cord             */

/* The three rows, descending: row j (0 = hundreds, 1 = tens,
   2 = units) holds its nine signs, and sign i (0..8) is worth (i+1)
   times the row's scale.  The descent rank is 3 - j. */
static const uint32_t row_hund[9] = {0x3C1u, 0x3C3u, 0x3C4u, 0x3C5u, 0x3C6u,
                                     0x3C7u, 0x3C8u, 0x3C9u, 0x3E1u};
static const uint32_t row_ten[9] = {0x3B9u, 0x3BAu, 0x3BBu, 0x3BCu, 0x3BDu,
                                    0x3BEu, 0x3BFu, 0x3C0u, 0x3DFu};
static const uint32_t row_unit[9] = {0x3B1u, 0x3B2u, 0x3B3u, 0x3B4u, 0x3B5u,
                                     0x3DBu, 0x3B6u, 0x3B7u, 0x3B8u};
static const uint32_t* const rows[3] = {row_hund, row_ten, row_unit};

/* Count the live digits of k (k <= 999): each nonzero decimal digit
   gets its row's dedicated sign, once; the blank digit writes
   nothing. */
static unsigned run_signs(uint64_t k) {
    unsigned c = 0u;

    while (k != 0u) {
        if ((k % 10u) != 0u)
            c++;
        k /= 10u;
    }
    return c;
}

/* The canonical 2-byte UTF-8 of one BMP code point below 0x800 —
   every sign in the tables above obeys; the two keraia are CD
   B4/B5. */
static void put_sign(char* buf, size_t* w, uint32_t cp) {
    buf[(*w)++] = (char) (0xC0u | ((cp >> 6) & 0x1Fu));
    buf[(*w)++] = (char) (0x80u | (cp & 0x3Fu));
}

/* Write the run of k (k <= 999): rows descending, one sign per live
   digit. */
static void put_run(uint64_t k, char* buf, size_t* w) {
    static const uint64_t scales[3] = {100u, 10u, 1u};
    unsigned j;

    for (j = 0u; j < 3u; j++) {
        unsigned d = (unsigned) ((k / scales[j]) % 10u);

        if (d != 0u)
            put_sign(buf, w, rows[j][d - 1u]);
    }
}

size_t pn_greek_render_size(uint64_t n) {
    /* This answer never fails.    Outside 1..PN_GREEK_MAX_VALUE it
	 *answers 0 — the blank cord — because refusing the number is
	  *render's job, not the counter's. */
    uint64_t th, un;
    size_t bytes;

    if (n == 0u || n > PN_GREEK_MAX_VALUE)
        return 0u;
    th = n / 1000u;
    un = n % 1000u;
    bytes = 2u * (size_t) (run_signs(th) + run_signs(un));
    if (th != 0u)
        bytes += 2u; /*the low keraia             */
    bytes += 2u;     /*the trailing keraia        */
    if (th != 0u && un != 0u)
        bytes += 1u; /*the run-separating space   */
    return bytes;
}

bool pn_greek_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                     pn_error* _Nullable* _Nullable error) {
    if (buf == NULL)
        return pn_error_set(error, PN_ERROR_NULL, "render needs a buffer to write into");
    if (n > PN_GREEK_MAX_VALUE)
        return pn_error_set(error, PN_ERROR_RANGE,
                            "%llu runs past %llu: the alphabet"
                            " counts no higher",
                            (unsigned long long) n, (unsigned long long) PN_GREEK_MAX_VALUE);
    if (buflen < pn_greek_render_size(n) + 1u)
        return pn_error_set(error, PN_ERROR_BUFFER,
                            "buffer holds %zu byte(s); this"
                            " cord needs %zu for what it"
                            " holds plus a terminating 0.",
                            buflen, pn_greek_render_size(n));

    uint64_t th = n / 1000u;
    uint64_t un = n % 1000u;
    size_t w = 0u;

    if (n != 0u) {
        if (th != 0u) {
            put_sign(buf, &w, GREEK_LOWK);
            put_run(th, buf, &w);
            if (un != 0u)
                buf[w++] = ' ';
        }
        put_run(un, buf, &w);
        put_sign(buf, &w, GREEK_DEXIA);
    }
    buf[w] = '\0';
    return true;
}

/* Look one decoded code point up in the three rows.    The descent
   rank @rank (3, 2, 1: the order a run must walk) and the worth
   @val come back only for a sign with a table entry.    Everything
   else - the capitals, the lunate and final sigmas, the digamma, the
   tonos, the prime twin, Latin letters - misses and turns into a
   syntax error. */
static int sign_info(uint32_t cp, unsigned* rank, unsigned* val) {
    static const uint64_t scales[3] = {100u, 10u, 1u};
    unsigned j, i;

    for (j = 0u; j < 3u; j++) {
        uint64_t scale = scales[j];

        for (i = 0u; i < 9u; i++) {
            if (cp == rows[j][i]) {
                *rank = 3u - j;
                *val = (unsigned) ((i + 1u) * scale);
                return 1;
            }
        }
    }
    return 0;
}

/* The worth of a legal run — at most one sign per row, the rows
   strictly descending, every sign pulled from the tables - or 0 when
   the run is empty or malformed.  No run of a legal number is worth
   0: a blank row writes nothing and a written row is worth its
   sign. */
static uint64_t run_value(const uint32_t* toks, size_t len) {
    uint64_t sum = 0u;
    unsigned prev_rank = 4u;
    size_t i;

    for (i = 0u; i < len; i++) {
        unsigned rank, val;

        if (toks[i] == GREEK_LOWK || toks[i] == GREEK_DEXIA || toks[i] == GREEK_SPACE ||
            !sign_info(toks[i], &rank, &val) || rank >= prev_rank)
            return 0u;
        prev_rank = rank;
        sum += val;
    }
    return sum;
}

/* Parsing reads back what rendering wrote. */
bool pn_greek_parse(const char* _Nullable buf, size_t buflen, uint64_t* _Nullable out,
                    pn_error* _Nullable* _Nullable error) {
    if (buf == NULL || out == NULL)
        return pn_error_set(error, PN_ERROR_NULL,
                            "parse needs a string and a place to"
                            " put the number");
    if (buflen == 0u) {
        *out = 0u; /* the blank cord honors zero       */
        return true;
    }

    /* First pass: decode the signs, 2 bytes each.    Anything with
	  *no table entry below — a lone byte under 0x80 (Latin, NUL), a
	  *run that is too long, a continuation byte with no lead, a
	  *3-byte sequence, or any other shape that is canonical but
	  *foreign — is rejected right here, and the sign table rejects
	  *the stranger further down. */
    uint32_t toks[GREEK_MAX_TOKS];
    size_t n_tok = 0u, i = 0u, body, k;
    int low, sp_at = -1;

    while (i < buflen) {
        unsigned char b = (unsigned char) buf[i];

        if (n_tok >= GREEK_MAX_TOKS)
            return pn_error_set(error, PN_ERROR_SYNTAX,
                                "%zu token(s) is the most"
                                " a cord carries; a sign"
                                " more is no number",
                                (size_t) GREEK_MAX_TOKS);
        if (b == ' ') {
            toks[n_tok++] = GREEK_SPACE;
            i++;
        }
        else if (b < 0xC0u || b > 0xDFu || i + 2u > buflen || (buf[i + 1] & 0xC0) != 0x80) {
            return pn_error_set(error, PN_ERROR_SYNTAX,
                                "byte %zu does not open a"
                                " two-byte numeral sign",
                                i);
        }
        else {
            toks[n_tok++] =
                ((uint32_t) (b & 0x1Fu) << 6) | (uint32_t) ((unsigned char) buf[i + 1] & 0x3Fu);
            i += 2u;
        }
    }

    /* The trailing keraia sits at the very end and nowhere else. */
    if (toks[n_tok - 1u] != GREEK_DEXIA)
        return pn_error_set(error, PN_ERROR_SYNTAX,
                            "a cord ends with the trailing"
                            " keraia and nothing after it");
    body = n_tok - 1u;
    for (k = 0u; k < body; k++)
        if (toks[k] == GREEK_DEXIA)
            return pn_error_set(error, PN_ERROR_SYNTAX,
                                "a trailing keraia in the"
                                " middle of the cord: that"
                                " marks a number in the"
                                " middle");
    if (body == 0u)
        return pn_error_set(error, PN_ERROR_SYNTAX, "a lone keraia names nothing");

    /* The low keraia announces the cord from the front, exactly
	  *once. */
    low = (toks[0] == GREEK_LOWK);
    i = low ? 1u : 0u;
    if (i >= body)
        return pn_error_set(error, PN_ERROR_SYNTAX, "keraia wrapped around nothing");
    for (k = i; k < body; k++)
        if (toks[k] == GREEK_LOWK)
            return pn_error_set(error, PN_ERROR_SYNTAX,
                                "a second announcement in"
                                " one cord: the low"
                                " keraia comes but once");

    /* The separator: at most one, touching neither edge, and only
	  *under the low keraia — a cord below 1000 has no second run and
	  *no gutter to spare.    @sp_at keeps the TOKEN index of the
	  *space, not a byte offset. */
    for (k = i; k < body; k++) {
        if (toks[k] != GREEK_SPACE)
            continue;
        if (sp_at >= 0 || !low || k == i || k + 1u == body)
            return pn_error_set(error, PN_ERROR_SYNTAX,
                                "the separating space sits"
                                " in the wrong spot"
                                " (token %zu)",
                                k);
        sp_at = (int) k;
    }

    if (sp_at >= 0) {
        uint64_t pre = run_value(&toks[i], (size_t) sp_at - i);
        uint64_t suf = run_value(&toks[(size_t) sp_at + 1u], body - (size_t) sp_at - 1u);

        if (pre == 0u || suf == 0u)
            return pn_error_set(error, PN_ERROR_SYNTAX,
                                "the space cuts the cord"
                                " into a run that is not"
                                " whole");
        *out = pre * 1000u + suf;
        return true;
    }

    uint64_t v = run_value(&toks[i], body - i);

    if (v == 0u)
        return pn_error_set(error, PN_ERROR_SYNTAX,
                            "the run between the keraia holds"
                            " no well-formed row of signs");
    *out = low ? v * 1000u : v;
    return true;
}
