/*
    greek.c — Greek alphabetic numerals in UTF-8, from 1 to 999 999.

    The ancient Greeks wrote numbers with letters. Twenty-seven
    letters stand for 1 to 9, 10 to 90 and 100 to 900 (three of them
    are old letters kept just for numbers: digamma or stigma for 6,
    koppa for 90, sampi for 900). Like Roman and Egyptian numerals,
    the system adds up values instead of using place value, so this
    file converts straight between a uint64_t and text.

    A group of up to three letters writes 1 to 999: at most one
    hundreds letter, then one tens letter, then one ones letter, with
    zero digits left out. 999 is sampi, koppa, theta: ϡϟθ.  Each
    letter is written once; 900 is one sampi, not nine of anything.

    Thousands use the same letters with a small mark in front, the
    lower keraia (U+0375 ͵). A number always ends with the keraia
    (U+0374 ʹ), which marks the letters as a number, not a word:

    23     = κγʹ
    1000   = ͵αʹ
    1001   = ͵α αʹ      (thousands group, space, ones group)
    11000  = ͵ιαʹ

    This module puts one space between the thousands group and the
    rest. Without it, 10 001 (͵ι then α) and 11 000 (͵ια) would be
    the same letters.

    Every sign is 2 bytes in UTF-8. The reader accepts only the 27
    numeral letters and the two keraia; capitals, other sigma forms,
    accent marks and look-alike characters are syntax errors.

    Further reading:
    - https://en.wikipedia.org/wiki/Greek_numerals
    - https://en.wikipedia.org/wiki/Keraia
*/
#include <assert.h>
#include <string.h>

#include "pantheon.h"

#define GREEK_LOWK     0x375u /* the lower keraia: marks the thousands      */
#define GREEK_DEXIA    0x374u /* the keraia at the end: marks a number      */
#define GREEK_SPACE    0x020u /* the space between the two groups           */
/* lower keraia, 3 letters, space, 3 letters, keraia: at most nine signs */
#define GREEK_MAX_TOKS 12u

/*
    The three rows of letters: row j (0 hundreds, 1 tens, 2 ones)
    holds nine letters, and letter i (0 to 8) is worth (i + 1) times
    the row's value. A group must use the rows in order, so each row
    gets a rank, 3 - j.
*/
static const uint32_t row_hund[9] = {0x3C1u, 0x3C3u, 0x3C4u, 0x3C5u, 0x3C6u,
                                     0x3C7u, 0x3C8u, 0x3C9u, 0x3E1u};
static const uint32_t row_ten[9] = {0x3B9u, 0x3BAu, 0x3BBu, 0x3BCu, 0x3BDu,
                                    0x3BEu, 0x3BFu, 0x3C0u, 0x3DFu};
static const uint32_t row_unit[9] = {0x3B1u, 0x3B2u, 0x3B3u, 0x3B4u, 0x3B5u,
                                     0x3DBu, 0x3B6u, 0x3B7u, 0x3B8u};
static const uint32_t* const rows[3] = {row_hund, row_ten, row_unit};

/*
    The number of letters needed for k (k <= 999): one for each
    nonzero digit.
*/
static unsigned run_signs(uint64_t k) {
    unsigned c = 0u;

    while (k != 0u) {
        if ((k % 10u) != 0u) {
            c++;
        }
        k /= 10u;
    }
    return c;
}

/*
    The 2-byte UTF-8 form of a code point below 0x800. Every letter in
    the tables uses it; the two keraia are CD B4 and CD B5.
    https://en.wikipedia.org/wiki/UTF-8#Encoding
*/
static void put_sign(char* buf, size_t* w, uint32_t cp) {
    buf[(*w)++] = (char) (0xC0u | ((cp >> 6) & 0x1Fu));
    buf[(*w)++] = (char) (0x80u | (cp & 0x3Fu));
}

/* Write the letters for k (k <= 999): one per nonzero digit, biggest first. */
static void put_run(uint64_t k, char* buf, size_t* w) {
    static const uint64_t scales[3] = {100u, 10u, 1u};
    unsigned j;

    for (j = 0u; j < 3u; j++) {
        unsigned d = (unsigned) ((k / scales[j]) % 10u);

        if (d != 0u) {
            put_sign(buf, w, rows[j][d - 1u]);
        }
    }
}

size_t pn_greek_render_size(uint64_t n) {
    /*
        Never fails. Outside 1..PN_GREEK_MAX_VALUE it returns 0;
        pn_greek_render is the function that reports the error.
    */
    uint64_t th, un;
    size_t bytes;

    if (n == 0u || n > PN_GREEK_MAX_VALUE) {
        return 0u;
    }
    th = n / 1000u;
    un = n % 1000u;
    bytes = 2u * (size_t) (run_signs(th) + run_signs(un));
    if (th != 0u) {
        bytes += 2u; /* the lower keraia          */
    }
    bytes += 2u; /* the keraia at the end     */
    if (th != 0u && un != 0u) {
        bytes += 1u; /* the space between groups  */
    }
    return bytes;
}

char* _Nullable pn_greek_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                                pn_error* _Nullable* _Nullable error) {
    assert(buf);
    if (n > PN_GREEK_MAX_VALUE) {
        if (error) {
            *error = pn_error_init(PN_ERROR_RANGE,
                                   "%llu runs past %llu: the alphabet"
                                   " counts no higher",
                                   (unsigned long long) n, (unsigned long long) PN_GREEK_MAX_VALUE);
        }
        return NULL;
    }
    if (buflen < pn_greek_render_size(n) + 1u) {
        if (error) {
            *error = pn_error_init(PN_ERROR_BUFFER,
                                   "buffer holds %zu byte(s); this"
                                   " cord needs %zu for what it"
                                   " holds plus a terminating 0.",
                                   buflen, pn_greek_render_size(n));
        }
        return NULL;
    }

    uint64_t th = n / 1000u;
    uint64_t un = n % 1000u;
    size_t w = 0u;

    if (n != 0u) {
        if (th != 0u) {
            put_sign(buf, &w, GREEK_LOWK);
            put_run(th, buf, &w);
            if (un != 0u) {
                buf[w++] = ' ';
            }
        }
        put_run(un, buf, &w);
        put_sign(buf, &w, GREEK_DEXIA);
    }
    buf[w] = '\0';
    return buf;
}

/*
    Find a code point in the three rows. On a match, returns its
    rank (3, 2 or 1, the order a group must follow) in *rank and its
    value in *val. Anything else (capitals, other sigma forms,
    digamma, accent marks, Latin letters) doesn't match and becomes a
    syntax error.
*/
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

/*
    The value of a group of letters, or 0 if the group is empty or
    badly formed. A well-formed group is at most one letter per row,
    rows in order. A real group is never worth 0, so 0 can safely
    mean "bad".
*/
static uint64_t run_value(const uint32_t* toks, size_t len) {
    uint64_t sum = 0u;
    unsigned prev_rank = 4u;
    size_t i;

    for (i = 0u; i < len; i++) {
        unsigned rank, val;

        if (toks[i] == GREEK_LOWK || toks[i] == GREEK_DEXIA || toks[i] == GREEK_SPACE
            || !sign_info(toks[i], &rank, &val) || rank >= prev_rank) {
            return 0u;
        }
        prev_rank = rank;
        sum += val;
    }
    return sum;
}

/* Reading is the reverse of writing. */
uint64_t pn_greek_parse(const char* _Nonnull text, pn_error* _Nullable* _Nullable error) {
    assert(text);

    size_t text_len = strlen(text);
    if (text_len == 0u) {
        return 0u; /* an empty string reads as 0      */
    }

    /*
        First pass: decode the signs, 2 bytes each. Rejected here: a
        single byte below 0x80 (Latin letters, the null terminator), a sign cut
        short, a stray continuation byte, a 3-byte sequence, or any
        other wrong shape. Characters that decode fine but aren't
        numeral signs are rejected later by the table lookup.
    */
    uint32_t toks[GREEK_MAX_TOKS];
    size_t n_tok = 0u, i = 0u, body, k;
    int low, sp_at = -1;

    while (i < text_len) {
        unsigned char b = (unsigned char) text[i];

        if (n_tok >= GREEK_MAX_TOKS) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "%zu token(s) is the most"
                                       " a cord carries; a sign"
                                       " more is no number",
                                       (size_t) GREEK_MAX_TOKS);
            }
            return 0;
        }
        if (b == ' ') {
            toks[n_tok++] = GREEK_SPACE;
            i++;
        }
        else if (b < 0xC0u || b > 0xDFu || i + 2u > text_len || (text[i + 1] & 0xC0) != 0x80) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "byte %zu does not open a"
                                       " two-byte numeral sign",
                                       i);
            }
            return 0;
        }
        else {
            toks[n_tok++] =
                ((uint32_t) (b & 0x1Fu) << 6) | (uint32_t) ((unsigned char) text[i + 1] & 0x3Fu);
            i += 2u;
        }
    }

    /* The keraia goes at the very end and nowhere else. */
    if (toks[n_tok - 1u] != GREEK_DEXIA) {
        if (error) {
            *error = pn_error_init(PN_ERROR_SYNTAX, "a cord ends with the trailing"
                                                    " keraia and nothing after it");
        }
        return 0;
    }
    body = n_tok - 1u;
    for (k = 0u; k < body; k++) {
        if (toks[k] == GREEK_DEXIA) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX, "a trailing keraia in the"
                                                        " middle of the cord: that"
                                                        " marks a number in the"
                                                        " middle");
            }
            return 0;
        }
    }
    if (body == 0u) {
        if (error) {
            *error = pn_error_init(PN_ERROR_SYNTAX, "a lone keraia names nothing");
        }
        return 0;
    }

    /* The lower keraia, if present, comes first, and only once. */
    low = (toks[0] == GREEK_LOWK);
    i = low ? 1u : 0u;
    if (i >= body) {
        if (error) {
            *error = pn_error_init(PN_ERROR_SYNTAX, "keraia wrapped around nothing");
        }
        return 0;
    }
    for (k = i; k < body; k++) {
        if (toks[k] == GREEK_LOWK) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX, "a second announcement in"
                                                        " one cord: the low"
                                                        " keraia comes but once");
            }
            return 0;
        }
    }

    /*
        The space: at most one, never first or last, and only after a
        lower keraia, since a number below 1000 has only one group.
        sp_at is the index of the space among the signs, not a byte
        offset.
    */
    for (k = i; k < body; k++) {
        if (toks[k] != GREEK_SPACE) {
            continue;
        }
        if (sp_at >= 0 || !low || k == i || k + 1u == body) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "the separating space sits"
                                       " in the wrong spot"
                                       " (token %zu)",
                                       k);
            }
            return 0;
        }
        sp_at = (int) k;
    }

    if (sp_at >= 0) {
        uint64_t pre = run_value(&toks[i], (size_t) sp_at - i);
        uint64_t suf = run_value(&toks[(size_t) sp_at + 1u], body - (size_t) sp_at - 1u);

        if (pre == 0u || suf == 0u) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX, "the space cuts the cord"
                                                        " into a run that is not"
                                                        " whole");
            }
            return 0;
        }
        return pre * 1000u + suf;
    }

    uint64_t v = run_value(&toks[i], body - i);

    if (v == 0u) {
        if (error) {
            *error = pn_error_init(PN_ERROR_SYNTAX, "the run between the keraia holds"
                                                    " no well-formed row of signs");
        }
        return 0;
    }
    return low ? v * 1000u : v;
}
