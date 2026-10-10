/*
    base20.c — base-20 (vigesimal) numbers.

    The Maya counted in base 20, and their calendar and astronomy
    tables in the Dresden Codex use it. This file reads and writes
    base-20 values in a modern notation (like 1;12,9) and converts
    them to and from uint64_t and double. It works the same way as
    base60.c; see that file for the basic idea of "places".

    Neither the Maya nor the Babylonians wrote a radix point (the
    base-20 version of a decimal point) or a minus sign. Both are
    modern additions so the type can hold any value.

    One real difference from base 60 is which fractions are exact in
    a double. A fraction is exact in binary only if its bottom is a
    power of 2. 20 = 4 x 5, so 0;5 (5/20 = 1/4) is exact, but 0;1
    (1/20) repeats forever in binary. On the other hand, 20^12 is
    still below 2^53, so a double holds whole base-20 values up to
    12 places exactly, versus 8 places for base 60.

    Further reading:
    - https://en.wikipedia.org/wiki/Maya_numerals
    - https://en.wikipedia.org/wiki/Vigesimal
    - https://en.wikipedia.org/wiki/Dresden_Codex
*/
#include <assert.h>
#include <math.h>
#include <string.h>

#include "pantheon.h"

/*
    The powers of 20 from 20^0 to 20^14, written out exactly. 20^14
    (about 1.64 x 10^18) is the largest that fits in a uint64_t. A
    double stores every whole number exactly only up to 2^53, and
    20^12 (about 4.1 x 10^15) is the largest power of 20 below that.
    https://en.wikipedia.org/wiki/Double-precision_floating-point_format
*/
static const uint64_t pow20_exact[] = {
    1ULL,
    20ULL,
    400ULL,
    8000ULL,
    160000ULL,
    3200000ULL,
    64000000ULL,
    1280000000ULL,
    25600000000ULL,
    512000000000ULL,
    10240000000000ULL,
    204800000000000ULL,
    4096000000000000ULL,
    81920000000000000ULL,
    1638400000000000000ULL,
};
#define POW20_COUNT (sizeof pow20_exact / sizeof pow20_exact[0])

/*
    A value is well-formed when it has 1 to MAX places, every place
    is at most 19, and the radix is between 0 and n_places. Every
    function checks this first, so a struct filled in by hand with
    bad values gets an error instead of a wrong answer.
*/
static int well_formed(pn_base20 v) {
    if (v.n_places == 0 || v.n_places > PN_BASE20_MAX_PLACES) {
        return 0;
    }
    if (v.radix > v.n_places) {
        return 0;
    }
    for (unsigned i = 0; i < v.n_places; i++) {
        if (v.places[i] > 19) {
            return 0;
        }
    }
    return 1;
}

pn_base20* _Nullable pn_base20_parse(const char* _Nonnull text, pn_base20* _Nonnull out,
                                     pn_error* _Nullable* _Nullable error) {
    assert(text && out);

    pn_base20 v; /* built here; *out changes only if the whole parse succeeds */

    memset(&v, 0, sizeof v);

    const char* p = text;

    if (*p == '-') {
        v.neg = true;
        p++;
    }

    int radix = -1; /* where the ';' is, or -1 if there is none */

    for (;;) {
        /*
            One place: one or two decimal digits. A longer run of
            digits, such as a mistyped decimal "100", is read up to
            three digits and then rejected as out of range.
        */
        unsigned d = 0;
        int nd = 0;

        while (nd < 3 && *p >= '0' && *p <= '9') {
            d = d * 10u + (unsigned) (*p - '0');
            p++;
            nd++;
        }
        if (nd == 0) {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "a place needs one or two digits;"
                                       " found none at byte %zu of the"
                                       " string",
                                       (size_t) (p - text));
            }
            return NULL;
        }
        if (nd > 2 || d > 19u) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "place %u reads %u: a place holds"
                                       " 0 to 19, written with 1 to 2"
                                       " digits",
                                       v.n_places, d);
            }
            return NULL;
        }
        if (v.n_places == PN_BASE20_MAX_PLACES) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "a value holds at most %u places"
                                       " (20^%u runs past what the type"
                                       " stores)",
                                       PN_BASE20_MAX_PLACES, PN_BASE20_MAX_PLACES);
            }
            return NULL;
        }

        v.places[v.n_places++] = (uint8_t) d;

        if (*p == '\0') {
            break;
        }
        if (*p == ';') {
            if (radix >= 0) {
                if (error) {
                    *error = pn_error_init(PN_ERROR_SYNTAX,
                                           "a second ';' at byte %zu:"
                                           " the string gets one radix"
                                           " point at most",
                                           (size_t) (p - text));
                }
                return NULL;
            }
            radix = v.n_places; /* the number of places before the radix point */
        }
        else if (*p != ',' && *p != ':') {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "byte %zu of the string must join"
                                       " places with ',' ':' or ';' or"
                                       " end the value",
                                       (size_t) (p - text));
            }
            return NULL;
        }
        p++;
    }

    v.radix = (uint8_t) (radix >= 0 ? radix : v.n_places);
    *out = v;
    return out;
}

char* _Nullable pn_base20_format(pn_base20 v, char sep, char* _Nonnull buf, size_t buflen,
                                 pn_error* _Nullable* _Nullable error) {
    assert(buf);
    if (sep != ',' && sep != ':' && sep != ';') {
        if (error) {
            *error = pn_error_init(PN_ERROR_SYNTAX,
                                   "places may be joined with ',' ':'"
                                   " or ';' only, not '%c'",
                                   sep);
        }
        return NULL;
    }
    if (!well_formed(v)) {
        if (error) {
            *error = pn_error_init(PN_ERROR_DOMAIN,
                                   "the value is not well-formed:"
                                   " %u places, radix %u - places need"
                                   " 1 to %u slots, each 0 to 19, and"
                                   " the radix cannot pass n_places",
                                   v.n_places, v.radix, PN_BASE20_MAX_PLACES);
        }
        return NULL;
    }

    /*
        Write into a scratch buffer first, so a failure never leaves
        buf holding half a number. The worst case is '-' + 16
        places x 2 digits + 15 separators + the 0 byte = 49 bytes,
        which fits in the declared maximum.

        Unlike pn_base60_format, places are written with as few
        digits as they need ("1,2,0,0", not "1,02,00,00"). Scholars
        pad base-60 places to two digits by convention, but there is
        no such convention for base 20. The first place still
        writes its 0, because "0;5" (1/4) needs it.
    */
    char tmp[PN_BASE20_FORMAT_MAX_LEN];
    char* q = tmp;

    if (v.neg) {
        *q++ = '-';
    }
    for (unsigned i = 0; i < v.n_places; i++) {
        if (i > 0) {
            *q++ = (i == v.radix && v.radix < v.n_places) ? ';' : sep;
        }
        unsigned d = v.places[i];

        if (d >= 10u) { /* a second digit only for 10 to 19 */
            *q++ = (char) ('0' + (int) (d / 10u));
        }
        *q++ = (char) ('0' + (int) (d % 10u));
    }

    size_t len = (size_t) (q - tmp);

    if (len + 1 > buflen) {
        if (error) {
            *error = pn_error_init(PN_ERROR_BUFFER,
                                   "buffer holds %zu bytes; this value"
                                   " needs %zu plus the terminating NUL",
                                   buflen, len);
        }
        return NULL;
    }
    memcpy(buf, tmp, len);
    buf[len] = '\0';
    return buf;
}

/*
    from_u64 finds the places from the bottom up: divide by 20, keep
    the remainder as the next place, repeat. to_u64 rebuilds the
    number with Horner's method: start at 0, then for each place
    multiply by 20 and add the place. Unlike the Babylonians, the
    Maya had a zero sign (a shell), so their numbers were never
    ambiguous about empty places.
    https://en.wikipedia.org/wiki/Horner%27s_method
*/
pn_base20* _Nullable pn_base20_from_u64(uint64_t n, pn_base20* _Nonnull out,
                                        pn_error* _Nullable* _Nullable error) {
    assert(out);

    pn_base20 v;

    memset(&v, 0, sizeof v);
    v.neg = false;

    uint8_t* p = v.places; /* found smallest place first... */
    size_t k = 0;

    do {
        if (k == PN_BASE20_MAX_PLACES) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "this many base-20 places will"
                                       " not fit the value (the guard"
                                       " reads as unreachable: 2^64"
                                       " needs 15 places, the type"
                                       " holds %u)",
                                       PN_BASE20_MAX_PLACES);
            }
            return NULL;
        }
        p[k++] = (uint8_t) (n % 20u);
        n /= 20u;
    } while (n != 0);
    /* ...then reversed, so the biggest place comes first */
    for (size_t i = 0; i < k / 2; i++) {
        uint8_t t = p[i];

        p[i] = p[k - 1 - i];
        p[k - 1 - i] = t;
    }
    v.n_places = (uint8_t) k;
    v.radix = (uint8_t) k;
    *out = v;
    return out;
}

uint64_t* _Nullable pn_base20_to_u64(pn_base20 v, uint64_t* _Nonnull out,
                                     pn_error* _Nullable* _Nullable error) {
    assert(out);
    if (v.n_places == 0 || v.n_places > PN_BASE20_MAX_PLACES || v.radix > v.n_places) {
        if (error) {
            *error = pn_error_init(PN_ERROR_DOMAIN,
                                   "the value is not well-formed:"
                                   " %u places, radix %u",
                                   v.n_places, v.radix);
        }
        return NULL;
    }
    if (v.radix != v.n_places) {
        if (error) {
            *error = pn_error_init(PN_ERROR_DOMAIN,
                                   "the value has %u fractional place(s)"
                                   " after the radix point, so it is no"
                                   " whole number",
                                   (unsigned) v.n_places - v.radix);
        }
        return NULL;
    }

    uint64_t acc = 0;

    for (unsigned i = 0; i < v.n_places; i++) {
        uint64_t d = v.places[i];

        if (d > 19u) {
            if (error) {
                *error = pn_error_init(PN_ERROR_DOMAIN,
                                       "place %u holds %llu: a place"
                                       " runs 0 to 19",
                                       i, (unsigned long long) d);
            }
            return NULL;
        }
        if (acc > (UINT64_MAX - d) / 20u) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "the value needs %u base-20"
                                       " places: more than a uint64_t"
                                       " can count",
                                       v.n_places);
            }
            return NULL;
        }
        acc = acc * 20u + d;
    }
    /* only the size: the sign is left to the caller, as in pn_base60_to_u64 */
    *out = acc;
    return out;
}

/*
    to_double computes the whole part and the fraction each as an
    exact uint64_t where possible, then does one division, which the
    floating-point hardware rounds correctly. So the answer is exact
    whenever a double can hold the value at all.
*/
double* _Nullable pn_base20_to_double(pn_base20 v, double* _Nonnull out,
                                      pn_error* _Nullable* _Nullable error) {
    assert(out);
    if (!well_formed(v)) {
        if (error) {
            *error = pn_error_init(PN_ERROR_DOMAIN,
                                   "the value is not well-formed:"
                                   " %u places, radix %u",
                                   v.n_places, v.radix);
        }
        return NULL;
    }

    double val = 0.0;

    /* the whole part, by Horner's method, exact while it fits in a uint64_t */
    int overflowed = 0;
    uint64_t acc = 0;
    unsigned i = 0;

    for (; i < v.radix; i++) {
        uint64_t d = v.places[i];

        if (acc > (UINT64_MAX - d) / 20u) {
            overflowed = 1;
            break;
        }
        acc = acc * 20u + d;
    }
    val = (double) acc;
    if (overflowed) { /* too big for a uint64_t: finish in double */
        for (; i < v.radix; i++) {
            val = val * 20.0 + (double) v.places[i];
        }
    }

    /*
        the fraction: the fraction places as one whole number F,
        divided by 20^k, where k is the number of fraction places.
        Both are exact while k <= 14, so one division gives a
        correctly rounded answer.
    */
    unsigned k = (unsigned) v.n_places - v.radix;

    if (k > 0) {
        if (k < POW20_COUNT) {
            uint64_t f = 0;

            for (i = v.radix; i < v.n_places; i++) {
                f = f * 20u + v.places[i];
            }
            val += (double) f / (double) pow20_exact[k];
        }
        else { /* too many places for exact whole numbers: divide by 20 place by place */
            double frac = 0.0;

            for (i = v.n_places; i-- > v.radix;) {
                frac = (frac + (double) v.places[i]) / 20.0;
            }
            val += frac;
        }
    }

    *out = v.neg ? -val : val;
    return out;
}

pn_base20* _Nullable pn_base20_from_double(double x, uint8_t frac_places, pn_base20* _Nonnull out,
                                           pn_error* _Nullable* _Nullable error) {
    assert(out);
    if (isnan(x) || isinf(x)) {
        if (error) {
            *error = pn_error_init(PN_ERROR_DOMAIN, "NaN and infinity name no number a"
                                                    " codex page could hold");
        }
        return NULL;
    }
    if ((int) frac_places > PN_BASE20_MAX_PLACES) {
        if (error) {
            *error = pn_error_init(PN_ERROR_DOMAIN,
                                   "asked for %u fractional places;"
                                   " the value holds %u places total",
                                   (unsigned) frac_places, PN_BASE20_MAX_PLACES);
        }
        return NULL;
    }

    double mag = fabs(x);
    double ip = floor(mag); /* the whole-number part */
    double fp = mag - ip;   /* the fractional part: at least 0 and less than 1 */

    /*
        The whole-number places, smallest first: ipd[0] is the
        ones place. ip / 20.0 is rounded by the hardware, so the
        remainder is recomputed and corrected instead of trusted.
    */
    uint8_t ipd[PN_BASE20_MAX_PLACES];
    size_t ki = 0;

    while (ip >= 1.0) {
        if (ki == PN_BASE20_MAX_PLACES) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "the whole part needs more than"
                                       " %u base-20 places",
                                       PN_BASE20_MAX_PLACES);
            }
            return NULL;
        }
        double q = floor(ip / 20.0);
        double r = ip - q * 20.0;

        if (r >= 20.0) {
            q += 1.0;
            r -= 20.0;
        }
        else if (r < 0.0) {
            q -= 1.0;
            r += 20.0;
        }
        ipd[ki++] = (uint8_t) r; /* r is a whole number from 0 to 19 */
        ip = q;
    }

    /*
        The fraction places, biggest first: fpd[0] is the 1/20
        place. Each step multiplies by 20 and takes off the whole
        part. The last kept place is rounded half away from zero
        (0.5 rounds up). Trailing zero places are kept, so the
        number of places is always what the caller asked for.
        https://en.wikipedia.org/wiki/Rounding
    */
    uint8_t fpd[PN_BASE20_MAX_PLACES];
    size_t kf = frac_places;

    for (size_t j = 0; j < kf; j++) {
        if (ki + j == PN_BASE20_MAX_PLACES) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "%u whole places plus this"
                                       " fraction will not share %u"
                                       " slots",
                                       (unsigned) ki, PN_BASE20_MAX_PLACES);
            }
            return NULL;
        }
        fp *= 20.0;
        uint8_t dgt = (uint8_t) fp; /* rounds down, since fp is never negative */

        fp -= (double) dgt;
        if (j + 1 == kf && fp >= 0.5) {
            dgt++; /* may reach 20; the carrying below fixes that */
        }
        fpd[j] = dgt;
    }

    /*
        Carrying, as in grade-school addition: a fraction place that
        reached 20 becomes 0 and adds 1 to the place on its left. A
        carry out of the first fraction place adds 1 to the whole
        part. If no fraction places were kept, the leftover
        fraction decides whether to round the whole part up.
    */
    int carry = (kf == 0 && fp >= 0.5);

    for (size_t i = kf; i-- > 0;) {
        if (fpd[i] == 20) {
            fpd[i] = 0;
            if (i > 0) {
                fpd[i - 1]++;
            }
            else {
                carry = 1;
            }
        }
    }

    /*
        Carry through the whole-number places, smallest first. If
        the carry goes past the biggest place (as in 9 + 1 = 10 in
        base 10), the value gains a new leading 1 and every other
        place is already 0.
    */
    int carry_out = 0;

    if (carry) {
        size_t j = 0;

        for (;;) {
            if (j == ki) { /* carried past the biggest place */
                carry_out = 1;
                break;
            }
            if (++ipd[j] != 20) {
                break;
            }
            ipd[j++] = 0;
        }
    }

    pn_base20 v;

    memset(&v, 0, sizeof v);
    v.neg = signbit(x) != 0;

    size_t total = ki + kf;

    if (carry_out) {
        if (total == PN_BASE20_MAX_PLACES) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "rounding grew the value past"
                                       " its %u places; there is no"
                                       " room for the new leading"
                                       " place",
                                       PN_BASE20_MAX_PLACES);
            }
            return NULL;
        }
        v.places[0] = 1;
        total++; /* all the other places stay 0 */
        v.radix = (uint8_t) (ki + 1);
    }
    else {
        for (size_t i = 0; i < ki; i++) {
            /* whole-number places, reversed to biggest first */
            v.places[i] = ipd[ki - 1 - i];
        }
        for (size_t i = 0; i < kf; i++) {
            /* fraction places are already biggest first */
            v.places[ki + i] = fpd[i];
        }
        v.radix = (uint8_t) ki;
        if (total == 0) {
            total = 1; /* everything rounded to 0: one place, {0} */
        }
    }

    v.n_places = (uint8_t) total;
    *out = v;
    return out;
}
