/*
    roman.c — Roman numerals in ASCII, from 1 to 3999.

    Roman numerals have no place values: you add up the letters, with
    a few pairs where a smaller letter before a bigger one means
    subtract (IV = 4, IX = 9, XL = 40, XC = 90, CD = 400, CM = 900).
    So, like the Egyptian and quipu modules, this file converts
    straight between a uint64_t and text, with no struct in between.

    Writing a number: each decimal digit becomes a fixed group of
    letters, one table per place (thousands, hundreds, tens, units).
    1994 is M + CM + XC + IV = MCMXCIV. This is the standard modern
    form: no IIII for 4 (seen on some clock faces), no IC for 99. The
    largest number it can write is 3999, MMMCMXCIX.

    Reading a number: go left to right, each time taking the biggest
    letter or subtractive pair that fits, and add up the values. That
    alone would also accept bad spellings such as IIII or VIV, so the
    result is then written back out and compared with the input. If
    they don't match exactly, the input was not standard and is
    rejected. Using the writer as the judge means the reader and the
    writer can never disagree.

    Further reading:
    - https://en.wikipedia.org/wiki/Roman_numerals
*/
#include <assert.h>
#include <string.h>

#include "pantheon.h"

/*
    The letters for each digit, one table per decimal place. The
    table index is the digit; the thousands table only goes to 3.
*/
static const char* const th_row[4] = {"", "M", "MM", "MMM"};
static const char* const h_row[10] = {"", "C", "CC", "CCC", "CD", "D", "DC", "DCC", "DCCC", "CM"};
static const char* const t_row[10] = {"", "X", "XX", "XXX", "XL", "L", "LX", "LXX", "LXXX", "XC"};
static const char* const u_row[10] = {"", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"};

size_t pn_roman_render_size(uint64_t n) {
    /*
        Never fails: for n outside 1..PN_ROMAN_MAX_VALUE it returns 0,
        and pn_roman_render is the function that reports the error.
    */
    if (n == 0u || n > PN_ROMAN_MAX_VALUE) {
        return 0u;
    }
    return strlen(th_row[n / 1000u]) + strlen(h_row[(n / 100u) % 10u])
           + strlen(t_row[(n / 10u) % 10u]) + strlen(u_row[n % 10u]);
}

/* Append one table entry's letters to buf at position *w. */
static void put_row(const char* row, char* buf, size_t* w) {
    while (*row != '\0') {
        buf[(*w)++] = *row++;
    }
}

char* _Nullable pn_roman_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                                pn_error* _Nullable* _Nullable error) {
    assert(buf);
    if (n > PN_ROMAN_MAX_VALUE) {
        if (error) {
            *error = pn_error_init(PN_ERROR_RANGE,
                                   "%llu runs past %llu: the tally"
                                   " walks no higher than MMM CM XC"
                                   " IX",
                                   (unsigned long long) n, (unsigned long long) PN_ROMAN_MAX_VALUE);
        }
        return NULL;
    }
    if (buflen < pn_roman_render_size(n) + 1u) {
        if (error) {
            *error = pn_error_init(PN_ERROR_BUFFER,
                                   "buffer holds %zu byte(s); this"
                                   " numeral needs %zu plus a"
                                   " terminating 0 byte",
                                   buflen, pn_roman_render_size(n));
        }
        return NULL;
    }

    size_t w = 0u;

    put_row(th_row[n / 1000u], buf, &w);
    put_row(h_row[(n / 100u) % 10u], buf, &w);
    put_row(t_row[(n / 10u) % 10u], buf, &w);
    put_row(u_row[n % 10u], buf, &w);
    buf[w] = '\0';
    return buf;
}

/*
    Every letter and subtractive pair with its value, biggest first,
    for the left-to-right reading in pn_roman_parse.
*/
static const struct {
    uint64_t value;
    char text[3];
} canon[] = {
    {1000u, "M"}, {900u, "CM"}, {500u, "D"}, {400u, "CD"}, {100u, "C"}, {90u, "XC"}, {50u, "L"},
    {40u, "XL"},  {10u, "X"},   {9u, "IX"},  {5u, "V"},    {4u, "IV"},  {1u, "I"},
};
#define N_CANON ((sizeof canon) / (sizeof canon[0]))

uint64_t* _Nullable pn_roman_parse(const char* _Nonnull buf, size_t buflen, uint64_t* _Nonnull out,
                                   pn_error* _Nullable* _Nullable error) {
    assert(buf && out);
    if (buflen == 0u) {
        *out = 0u; /* the Romans had no zero numeral; medieval writers used "nulla" */
        return out;
    }

    /*
        Take the biggest letter or pair the rest of the string starts
        with, add its value, and move past it. Any byte with no match
        (a space, a lowercase letter, a non-ASCII byte) is an error.
    */
    uint64_t sum = 0u;
    size_t i = 0u;

    while (i < buflen) {
        int matched = 0;

        for (unsigned k = 0u; k < N_CANON; k++) {
            size_t len = strlen(canon[k].text);

            if (i + len <= buflen && memcmp(buf + i, canon[k].text, len) == 0) {
                sum += canon[k].value;
                i += len;
                matched = 1;
                break;
            }
        }
        if (!matched) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "position %zu starts no"
                                       " Roman token: a run of"
                                       " letters the canon does"
                                       " not spell ends the walk",
                                       i);
            }
            return NULL;
        }
    }

    /*
        Write the sum back out as a numeral and compare. A non-standard
        spelling (IIII, VIV, XCX) adds up to a valid number but writes
        back differently, so it is caught here. An empty string never
        gets this far: it was handled above as 0.
    */
    char back[PN_ROMAN_FORMAT_MAX_LEN];
    pn_error* inner = NULL;
    bool ok = pn_roman_render(sum, back, sizeof back, &inner);

    pn_error_free(inner); /* the comparison below already tells us what we need */
    if (!ok || strlen(back) != buflen || memcmp(back, buf, buflen) != 0) {
        if (error) {
            *error =
                pn_error_init(PN_ERROR_SYNTAX,
                              "\"%.*s\" does not spell the"
                              " canon for %llu: the tally"
                              " must render back to itself",
                              (int) (buflen > 64 ? 64 : buflen), buf, (unsigned long long) sum);
        }
        return NULL;
    }

    *out = sum;
    return out;
}
