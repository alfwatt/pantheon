/*  fraction.c — libPantheon: common fractions.

    A numerator over a denominator, both uint64_t, the sign kept
    apart.  The road in from double is the continued fraction: unroll
    the value one term at a time into its convergents, stop at the
    denominator bound, and weigh the last semiconvergent that still
    fits.  Success allocates nothing except in pn_fraction_format,
    which hands its buffer to the caller.

    References: https://en.wikipedia.org/wiki/Continued_fraction ,
    https://en.wikipedia.org/wiki/Continued_fraction#Best_rational_approximations
*/
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pantheon.h"

/* 2^64, the first double that no longer fits a uint64_t */
#define PN_FRACTION_TWO_64 18446744073709551616.0

/* A finite double is exactly m / 2^s with m < 2^53, so its continued
   fraction is Euclid's ladder on two whole numbers - but 2^s runs as
   deep as 2^1074.  Past s = 120 the value sits below 2^-67, nearer 0/1
   than any 1/k a uint64_t denominator allows, so 128 bits cover every
   ladder worth climbing. */
#define PN_FRACTION_MAX_SHIFT 120

__extension__ typedef unsigned __int128 pn_u128;

/* Euclid's ladder (Elements VII.2), kept private: the public
   pn_math_gcd reports through an out-param this file has no use for. */
static uint64_t gcd(uint64_t a, uint64_t b) {
    while (b != 0) {
        uint64_t r = a % b;

        a = b;
        b = r;
    }

    return a;
}

pn_fraction pn_fraction_make(uint64_t numerator, uint64_t denominator, bool neg) {
    assert(denominator != 0);

    uint64_t divisor = gcd(numerator, denominator); /* never 0: denominator is not */

    return (pn_fraction) {
        .numerator = numerator / divisor,
        .denominator = denominator / divisor,
        .neg = neg && numerator != 0, /* zero carries no sign */
    };
}

/* The sign of a/b - c/d for b, d > 0, by walking both continued
   fractions side by side: no product, so nothing overflows. */
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

    if (x >= PN_FRACTION_TWO_64) { /* past the ledger: saturate */
        return (pn_fraction) {.numerator = UINT64_MAX, .denominator = 1, .neg = neg};
    }
    if (x == 0.0) {
        return pn_fraction_make(0, 1, false);
    }

    /* x = m / 2^shift exactly, with the shift as small as m allows */
    int exponent = 0;
    uint64_t m = (uint64_t) ldexp(frexp(x, &exponent), 53); /* frexp's half-to-one, made whole */
    int shift = 53 - exponent;

    while (shift > 0 && (m & 1u) == 0) {
        m >>= 1;
        shift--;
    }
    if (shift <= 0) { /* a whole number, and under 2^64 */
        return pn_fraction_make(m << -shift, 1, neg);
    }
    if (shift > PN_FRACTION_MAX_SHIFT) {
        return pn_fraction_make(0, 1, false);
    }

    /*  The two latest convergents, h1/k1 the newer and h2/k2 the one
  before, seeded with the formal 1/0 and 0/1 so the recurrence
  h = a*h1 + h2, k = a*k1 + k2 starts the first term as a/1.
*/
    pn_u128 num = m;
    pn_u128 den = (pn_u128) 1 << shift;
    uint64_t h1 = 1, k1 = 0;
    uint64_t h2 = 0, k2 = 1;

    for (;;) {
        pn_u128 a = num / den;
        pn_u128 rem = num - a * den;

        /* the largest term that keeps k within the bound; h never
           overflows, every numerator on the ladder stays at or under m */
        pn_u128 limit = k1 == 0 ? UINT64_MAX : (max_denominator - k2) / k1;

        /*  The next convergent would pass the bound: the semiconvergent
  t*h1 + h2 over t*k1 + k2 with the largest t that fits beats
  h1/k1 by the half rule - when 2t > a, or when 2t == a and
  k2/k1 > rem/den (the reversed and the remaining expansions
  weighed against each other).  Ties keep the convergent.
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

        if (rem == 0) { /* the ladder ends: h1/k1 is the value itself */
            break;      // for
        }
        num = den;
        den = rem;
    }

    /* convergents are already in lowest terms; make only settles the sign of zero */
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

/* Read one run of decimal digits at *cursor into *out; 0 when the run
   is empty, -1 when it tops UINT64_MAX, 1 on success. */
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
        if (side == 0 && *p == '/') {
            p++;
            continue;
        }
        if (error) {
            *error = pn_error_init(PN_ERROR_SYNTAX, "byte %zu of the string must %s",
                                   (size_t) (p - text),
                                   side == 0 ? "be a '/' or end the value" : "end the value");
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

    /*  Written in scratch space first, so a short buffer can never
  hold half a number.  Worst case: '-' + 20 digits + '/' + 20
  digits + NUL = 43 bytes, the declared maximum.
*/
    char tmp[PN_FRACTION_FORMAT_MAX_LEN];
    int written = value.denominator == 1
                      ? snprintf(tmp, sizeof tmp, "%s%llu", value.neg ? "-" : "",
                                 (unsigned long long) value.numerator)
                      : snprintf(tmp, sizeof tmp, "%s%llu/%llu", value.neg ? "-" : "",
                                 (unsigned long long) value.numerator,
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
