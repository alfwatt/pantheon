/*
    fraction.c — fractions as a numerator over a denominator.

    A pn_fraction stores a fraction like 355/113 as two whole numbers
    plus a sign, always reduced to lowest terms.

    The interesting part is turning a double into a fraction. Every
    number has a continued fraction: write it as a whole part plus 1
    over something, then do the same to the something, and so on.
    pi = 3 + 1/(7 + 1/(15 + 1/(1 + ...))). Stopping early gives a
    fraction close to the number, called a convergent: 3, 22/7,
    333/106, 355/113. These are the best possible approximations for
    their size. pn_fraction_from_double finds the closest fraction
    whose denominator is no bigger than a limit the caller picks.

    Nothing here allocates memory except pn_fraction_format, which
    returns a string the caller must free.

    Further reading:
    - https://en.wikipedia.org/wiki/Continued_fraction
    - https://en.wikipedia.org/wiki/Continued_fraction#Best_rational_approximations
*/
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pantheon.h"

#define FRACTION_SLASH_UTF8    "\u2044" /* ⁄ bytes: E2 81 84 */
#define DIVISION_SLASH_UTF8    "\u2215" /* ∕ bytes: E2 88 95 */
#define FULLWIDTH_SOLIDUS_UTF8 "\uFF0F" /* ／ bytes: EF BC 8F */

/* 2^64, the smallest double too big for a uint64_t */
#define PN_FRACTION_TWO_64 18446744073709551616.0

/*
    Every finite double is exactly m / 2^s for whole numbers m and s,
    with m < 2^53, so its continued fraction can be found with exact
    whole-number arithmetic (Euclid's algorithm) instead of rounded
    floating point. 2^s can be as large as 2^1074, though. When
    s > 120 the value is below 2^-67, closer to 0 than to any fraction
    1/k with k in a uint64_t, so the answer is just 0. Up to that
    point, 128-bit integers are big enough.
*/
#define PN_FRACTION_MAX_SHIFT 120

__extension__ typedef unsigned __int128 pn_u128;

pn_fraction pn_fraction_make(uint64_t numerator, uint64_t denominator, bool neg) {
    assert(denominator != 0);

    /* never 0, because the denominator isn't 0 */
    uint64_t divisor = pn_math_greatest_common_divisor(numerator, denominator);

    return (pn_fraction) {
        .numerator = numerator / divisor,
        .denominator = denominator / divisor,
        .neg = neg && numerator != 0, /* zero carries no sign */
    };
}

/*
    Compare a/b with c/d (b and d above 0): returns 1, 0 or -1. It
    compares the continued fractions term by term instead of
    cross-multiplying, so no product can overflow.
*/
static int compare_ratio(pn_u128 a, pn_u128 b, pn_u128 c, pn_u128 d) {
    assert(b != 0 && d != 0);

    for (;;) {
        pn_u128 whole_ab = a / b;
        pn_u128 whole_cd = c / d;

        if (whole_ab != whole_cd) {
            return whole_ab > whole_cd ? 1 : -1;
        }
        a -= whole_ab * b;
        c -= whole_cd * d;
        if (a == 0) {
            return c == 0 ? 0 : -1;
        }
        if (c == 0) {
            return 1;
        }

        /* both now below 1: a/b > c/d exactly when d/c > b/a */
        pn_u128 swap_a = a;
        pn_u128 swap_b = b;

        a = d;
        b = c;
        c = swap_b;
        d = swap_a;
    }
}

pn_fraction pn_fraction_from_double(double value, uint64_t max_denominator) {
    assert(isfinite(value) && max_denominator > 0);

    bool neg = signbit(value) != 0;
    double x = fabs(value);

    if (x >= PN_FRACTION_TWO_64) { /* too big for a uint64_t: return the largest value instead */
        return (pn_fraction) {.numerator = UINT64_MAX, .denominator = 1, .neg = neg};
    }
    if (x == 0.0) {
        return pn_fraction_make(0, 1, false);
    }

    /* x = m / 2^shift exactly, with shift as small as possible */
    int exponent = 0;
    uint64_t m = (uint64_t) ldexp(
        frexp(x, &exponent),
        53); /* frexp gives a value from 0.5 to 1; times 2^53 it is a whole number */
    int shift = 53 - exponent;

    while (shift > 0 && (m & 1u) == 0) {
        m >>= 1;
        shift--;
    }
    if (shift <= 0) { /* a whole number below 2^64 */
        return pn_fraction_make(m << -shift, 1, neg);
    }
    if (shift > PN_FRACTION_MAX_SHIFT) {
        return pn_fraction_make(0, 1, false);
    }

    /*
        The last two convergents: h1/k1 (newest) and h2/k2 (the one
        before). Each new term a gives the next convergent:
        h = a x h1 + h2, k = a x k1 + k2. Starting from 1/0 and 0/1
        makes the first convergent a/1.
    */
    pn_u128 num = m;
    pn_u128 den = (pn_u128) 1 << shift;
    uint64_t h1 = 1, k1 = 0;
    uint64_t h2 = 0, k2 = 1;

    for (;;) {
        pn_u128 a = num / den;
        pn_u128 rem = num - a * den;

        /*
            the largest term that keeps k within the limit; h can't
            overflow, since every numerator here is at most m
        */
        pn_u128 limit = k1 == 0 ? UINT64_MAX : (max_denominator - k2) / k1;

        /*
            The next convergent's denominator would be over the limit.
            A "semiconvergent" (t x h1 + h2) / (t x k1 + k2), using the
            largest t that fits, may still be closer than h1/k1. The
            "half rule" decides: it is closer when 2t > a, or when
            2t == a and k2/k1 > rem/den. On an exact tie, keep h1/k1.
        */
        if (a > limit) {
            uint64_t t = (uint64_t) limit;
            bool semi_wins = (pn_u128) t * 2 > a
                             || ((pn_u128) t * 2 == a && compare_ratio(k2, k1, rem, den) > 0);

            if (t > 0 && semi_wins) {
                h1 = t * h1 + h2;
                k1 = t * k1 + k2;
            }
            break; // for
        }

        uint64_t term = (uint64_t) a;
        uint64_t h = term * h1 + h2;
        uint64_t k = term * k1 + k2;

        h2 = h1;
        k2 = k1;
        h1 = h;
        k1 = k;

        if (rem == 0) { /* no remainder: h1/k1 is exactly the value */
            break;      // for
        }
        num = den;
        den = rem;
    }

    /* convergents are already in lowest terms; make only fixes the sign of 0 */
    return pn_fraction_make(h1, k1, neg);
}

bool pn_fraction_equals(pn_fraction left, pn_fraction right) {
    assert(left.denominator != 0 && right.denominator != 0);

    if (left.numerator == 0 || right.numerator == 0) { /* zero carries no sign */
        return left.numerator == right.numerator;
    }

    return left.neg == right.neg
           && (pn_u128) left.numerator * right.denominator
                  == (pn_u128) right.numerator * left.denominator;
}

double pn_fraction_to_double(pn_fraction value) {
    assert(value.denominator != 0);

    double magnitude = (double) value.numerator / (double) value.denominator;

    return value.neg ? -magnitude : magnitude;
}

/*
    Read decimal digits at *cursor into *out. Returns 0 if there are
    no digits, -1 if the number is bigger than UINT64_MAX, 1 on
    success.
*/
static int read_digits(const char* _Nonnull* _Nonnull cursor, uint64_t* _Nonnull out) {
    assert(cursor && *cursor && out);

    const char* p = *cursor;
    uint64_t acc = 0;

    if (*p < '0' || *p > '9') {
        return 0;
    }
    while (*p >= '0' && *p <= '9') {
        uint64_t d = (uint64_t) (*p - '0');

        if (acc > (UINT64_MAX - d) / 10u) {
            return -1;
        }
        acc = acc * 10u + d;
        p++;
    }
    *cursor = p;
    *out = acc;

    return 1;
}

/*
    How many bytes of p make up a slash: 3 for ⁄ FRACTION SLASH, ∕ DIVISION
    SLASH or ／ FULLWIDTH SOLIDUS, 1 for / SOLIDUS, 0 for anything else.
*/
static size_t slash_length(const char* _Nonnull p) {
    assert(p);
    if (strncmp(p, FRACTION_SLASH_UTF8, 3) == 0 || strncmp(p, DIVISION_SLASH_UTF8, 3) == 0
        || strncmp(p, FULLWIDTH_SOLIDUS_UTF8, 3) == 0) {
        return 3; /* strncmp stops at the null terminator, so a short string is safe */
    }

    return *p == '/' ? 1 : 0;
}

pn_fraction pn_fraction_parse(const char* _Nonnull text, pn_error* _Nullable* _Nullable error) {
    assert(text);

    const char* p = text;
    bool neg = false;
    uint64_t numerator = 0;
    uint64_t denominator = 1;

    if (*p == '-') {
        neg = true;
        p++;
    }

    for (int side = 0; side < 2; side++) {
        const char* start = p;
        int read = read_digits(&p, side == 0 ? &numerator : &denominator);

        if (read == 0) {
            if (error) {
                *error =
                    pn_error_init(PN_ERROR_SYNTAX,
                                  "the %s needs one or more digits;"
                                  " found none at byte %zu of the"
                                  " string",
                                  side == 0 ? "numerator" : "denominator", (size_t) (start - text));
            }
            return (pn_fraction) {0};
        }
        if (read < 0) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "the %s at byte %zu runs past"
                                       " %llu",
                                       side == 0 ? "numerator" : "denominator",
                                       (size_t) (start - text), (unsigned long long) UINT64_MAX);
            }
            return (pn_fraction) {0};
        }
        if (*p == '\0') {
            break; // for
        }
        size_t slash = slash_length(p);

        if (side == 0 && slash > 0) {
            p += slash;
            continue;
        }
        if (error) {
            *error = pn_error_init(
                PN_ERROR_SYNTAX, "byte %zu of the string must %s", (size_t) (p - text),
                side == 0 ? "be a slash (U+2044, U+2215, U+FF0F or '/') or end the value"
                          : "end the value");
        }
        return (pn_fraction) {0};
    }

    if (denominator == 0) {
        if (error) {
            *error = pn_error_init(PN_ERROR_DOMAIN, "a fraction over 0 names no number");
        }
        return (pn_fraction) {0};
    }

    return pn_fraction_make(numerator, denominator, neg);
}

char* _Nullable pn_fraction_format(pn_fraction value, pn_error* _Nullable* _Nullable error) {
    char* buffer = malloc(PN_FRACTION_FORMAT_MAX_LEN);

    if (buffer == NULL) {
        if (error) {
            *error = pn_error_init(PN_ERROR_BUFFER, "no room for a %u byte buffer",
                                   (unsigned) PN_FRACTION_FORMAT_MAX_LEN);
        }
        return NULL;
    }

    if (pn_fraction_format_into(value, buffer, PN_FRACTION_FORMAT_MAX_LEN, error) == NULL) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

char* _Nullable pn_fraction_format_into(pn_fraction value, char* _Nonnull buffer, size_t buflen,
                                        pn_error* _Nullable* _Nullable error) {
    assert(buffer);
    if (value.denominator == 0) {
        if (error) {
            *error = pn_error_init(PN_ERROR_DOMAIN, "a fraction over 0 names no number");
        }
        return NULL;
    }

    /*
        Write into a scratch buffer first, so a short buffer never
        holds half a number. The worst case is '-' + 20 digits + the
        3-byte ⁄ + 20 digits + the null terminator = 45 bytes, the declared
        maximum.
    */
    char tmp[PN_FRACTION_FORMAT_MAX_LEN];
    int written = value.denominator == 1
                      ? snprintf(tmp, sizeof tmp, "%s%llu", value.neg ? "-" : "",
                                 (unsigned long long) value.numerator)
                      : snprintf(tmp, sizeof tmp, "%s%llu" FRACTION_SLASH_UTF8 "%llu",
                                 value.neg ? "-" : "", (unsigned long long) value.numerator,
                                 (unsigned long long) value.denominator);

    assert(written > 0 && (size_t) written < sizeof tmp);

    size_t len = (size_t) written;

    if (len + 1 > buflen) {
        if (error) {
            *error = pn_error_init(PN_ERROR_BUFFER,
                                   "buffer holds %zu bytes; this value"
                                   " needs %zu plus the terminating NUL",
                                   buflen, len);
        }
        return NULL;
    }
    memcpy(buffer, tmp, len + 1);

    return buffer;
}
