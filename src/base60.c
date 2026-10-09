/*  sex60.c — libPantheon: the Babylonian base-60 core.

    Base-60 places, the scholarly way of writing them, and the roads
    to and from uint64_t and double.  Success allocates nothing: the
    biggest temporaries here are a 16-byte place array and a 50-byte
    scratch buffer.

    References: https://en.wikipedia.org/wiki/Sexagesimal ,
    https://en.wikipedia.org/wiki/Babylonian_numerals
*/
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "pantheon.h"

/* The powers of 60 spelled out exactly: 60^0 through 60^10.  60^10
   = 6.05e17 is the last one that fits a uint64_t (60^11 = 3.6e19
   would not).  Only up to 60^8 = 1.68e14 does one still fit inside
   2^53, the range where a double holds every whole number exactly. */
static const uint64_t pow60_exact[] = {
    1ULL,
    60ULL,
    3600ULL,
    216000ULL,
    12960000ULL,
    777600000ULL,
    46656000000ULL,
    2799360000000ULL,
    167961600000000ULL,
    10077696000000000ULL,
    604661760000000000ULL,
};
#define POW60_COUNT (sizeof pow60_exact / sizeof pow60_exact[0])

/* A value is well-formed when it has 1 to MAX places, every place at
   most 59, and a radix between 0 and n_places.  A caller who pokes
   at the struct by hand must not be able to push the other
   functions into misbehaving. */
static int well_formed(pn_base60 v) {
    if (v.n_places == 0 || v.n_places > PN_BASE60_MAX_PLACES) {
        return 0;
    }
    if (v.radix > v.n_places) {
        return 0;
    }
    for (unsigned i = 0; i < v.n_places; i++) {
        if (v.places[i] > 59) {
            return 0;
        }
    }
    return 1;
}

pn_base60 pn_base60_parse(const char* _Nonnull text, pn_error* _Nullable* _Nullable error) {
    assert(text);

    pn_base60 v; /* built here; a failure hands back a zeroed value and a note */

    memset(&v, 0, sizeof v);

    const char* p = text;
    if (*p == '-') {
        v.neg = true;
        p++;
    }

    int radix = -1; /* where the ';' sits, if it sits anywhere */
    for (;;) {
        /*  One place: one or two decimal digits.  A longer run of
  digits - the "3599" from the header docs - is read
  three digits deep and refused as out of range.
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
            return (pn_base60) {0};
        }
        if (nd > 2 || d > 59u) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "place %u reads %u: a place holds 0"
                                       " to 59, written with 1 to 2 digits"
                                       " \"%.*s...\" included %s",
                                       v.n_places, d, nd > 3 ? 3 : nd, p - nd,
                                       nd > 2 ? "(three or more digits)" : "(a place above 59)");
            }
            return (pn_base60) {0};
        }
        if (v.n_places == PN_BASE60_MAX_PLACES) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "a value holds at most %u places"
                                       " (60^%u runs past what the type"
                                       " stores)",
                                       PN_BASE60_MAX_PLACES, PN_BASE60_MAX_PLACES);
            }
            return (pn_base60) {0};
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
                return (pn_base60) {0};
            }
            radix = v.n_places; /* places left of the point */
        }
        else if (*p != ',' && *p != ':') {
            if (error) {
                *error = pn_error_init(PN_ERROR_SYNTAX,
                                       "byte %zu of the string must join"
                                       " places with ',' ':' or ';' or"
                                       " end the value",
                                       (size_t) (p - text));
            }
            return (pn_base60) {0};
        }
        p++;
    }

    v.radix = (uint8_t) (radix >= 0 ? radix : v.n_places);

    return v;
}

char* _Nullable pn_base60_format(pn_base60 value, char place_sep,
                                 pn_error* _Nullable* _Nullable error) {
    char* buffer = malloc(PN_BASE60_FORMAT_MAX_LEN);

    if (buffer == NULL) {
        if (error) {
            *error = pn_error_init(PN_ERROR_BUFFER, "no room for a %u byte buffer",
                                   (unsigned) PN_BASE60_FORMAT_MAX_LEN);
        }
        return NULL;
    }

    if (pn_base60_format_into(value, place_sep, buffer, PN_BASE60_FORMAT_MAX_LEN, error) == NULL) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

char* _Nullable pn_base60_format_into(pn_base60 value, char place_sep, char* _Nonnull buffer,
                                      size_t buflen, pn_error* _Nullable* _Nullable error) {
    assert(buffer);
    if (place_sep != ',' && place_sep != ':' && place_sep != ';') {
        if (error) {
            *error = pn_error_init(PN_ERROR_SYNTAX,
                                   "places may be joined with ',' ':'"
                                   " or ';' only, not '%c'",
                                   place_sep);
        }
        return NULL;
    }
    if (!well_formed(value)) {
        if (error) {
            *error = pn_error_init(PN_ERROR_DOMAIN,
                                   "the value is not well-formed:"
                                   " %u places, radix %u - places need"
                                   " 1 to %u slots, each 0 to 59, and"
                                   " the radix cannot pass n_places",
                                   value.n_places, value.radix, PN_BASE60_MAX_PLACES);
        }
        return NULL;
    }

    /*  Written in scratch space first, so a late failure can never
  leave buffer holding half a number.  Worst case: '-' + 16
  places x 2 digits + 15 separators + NUL = 49 bytes, which
  the declared maximum has room for.
*/
    char tmp[PN_BASE60_FORMAT_MAX_LEN];
    char* q = tmp;

    if (value.neg) {
        *q++ = '-';
    }
    for (unsigned i = 0; i < value.n_places; i++) {
        if (i > 0) {
            *q++ = (i == value.radix && value.radix < value.n_places) ? ';' : place_sep;
        }
        unsigned d = value.places[i];

        if (i > 0 || d >= 10) { /* the first place drops its leading zero */
            *q++ = (char) ('0' + (int) (d / 10u));
        }
        *q++ = (char) ('0' + (int) (d % 10u));
    }

    size_t len = (size_t) (q - tmp);

    if (len + 1 > buflen) {
        if (error) {
            *error = pn_error_init(PN_ERROR_BUFFER,
                                   "buffer holds %zu bytes; this value"
                                   " needs %zu plus the terminating"
                                   " NUL",
                                   buflen, len);
        }
        return NULL;
    }
    memcpy(buffer, tmp, len);
    buffer[len] = '\0';
    return buffer;
}

/* from_u64 peels places off the bottom (divide by 60 again and
   again); to_u64 builds the number back up with Horner's scheme.
   Babylonian clerks did exactly this on the abacus, and exactly this
   in the wrong order whenever we misread a tablet's missing zero. */
pn_base60* _Nullable pn_base60_from_u64(uint64_t n, pn_base60* _Nonnull out,
                                        pn_error* _Nullable* _Nullable error) {
    assert(out);

    pn_base60 v;

    memset(&v, 0, sizeof v);
    v.neg = false;

    uint8_t* p = v.places; /* filled least-significant first... */
    size_t k = 0;

    do {
        if (k == PN_BASE60_MAX_PLACES) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "this many base-60 places will not"
                                       " fit the value (the guard reads as"
                                       " unreachable: 2^64 needs 11 places,"
                                       " the type holds %u)",
                                       PN_BASE60_MAX_PLACES);
            }
            return NULL;
        }
        p[k++] = (uint8_t) (n % 60u);
        n /= 60u;
    } while (n != 0);
    /* ...then flipped around in place */
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

uint64_t* _Nullable pn_base60_to_u64(pn_base60 v, uint64_t* _Nonnull out,
                                     pn_error* _Nullable* _Nullable error) {
    assert(out);
    if (v.n_places == 0 || v.n_places > PN_BASE60_MAX_PLACES || v.radix > v.n_places) {
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

        if (d > 59u) {
            if (error) {
                *error = pn_error_init(PN_ERROR_DOMAIN,
                                       "place %u holds %u: a place runs 0"
                                       " to 59",
                                       i, (unsigned) d);
            }
            return NULL;
        }
        if (acc > (UINT64_MAX - d) / 60u) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "the value needs %u base-60 places:"
                                       " more than a uint64_t can count",
                                       v.n_places);
            }
            return NULL;
        }
        acc = acc * 60u + d;
    }
    *out = acc;
    return out;
}

/* to_double works out the whole part and the fraction each as an
   exact uint64_t where one fits, then does one correctly-rounded
   division.  So the answer is exact whenever a double is able to
   hold it at all. */
double* _Nullable pn_base60_to_double(pn_base60 v, double* _Nonnull out,
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

    /* the whole part: Horner's scheme, exact through uint64 range */
    int overflowed = 0;
    uint64_t acc = 0;
    unsigned i = 0;

    for (; i < v.radix; i++) {
        uint64_t d = v.places[i];

        if (acc > (UINT64_MAX - d) / 60u) {
            overflowed = 1;
            break;
        }
        acc = acc * 60u + d;
    }
    val = (double) acc;
    if (overflowed) { /* past uint64 territory: carry on in double */
        for (; i < v.radix; i++) {
            val = val * 60.0 + (double) v.places[i];
        }
    }

    /*  the fraction: one exact whole number F over one exact power
  60^k, which is a single correctly-rounded division while
  k <= 10.
*/
    unsigned k = (unsigned) v.n_places - v.radix;

    if (k > 0) {
        if (k < POW60_COUNT) {
            uint64_t f = 0;

            for (i = v.radix; i < v.n_places; i++) {
                f = f * 60u + v.places[i];
            }
            val += (double) f / (double) pow60_exact[k];
        }
        else { /* too deep down for exact integers */
            double frac = 0.0;

            for (i = v.n_places; i-- > v.radix;) {
                frac = (frac + (double) v.places[i]) / 60.0;
            }
            val += frac;
        }
    }

    *out = v.neg ? -val : val;
    return out;
}

pn_base60* _Nullable pn_base60_from_double(double x, uint8_t frac_places, pn_base60* _Nonnull out,
                                           pn_error* _Nullable* _Nullable error) {
    assert(out);
    if (isnan(x) || isinf(x)) {
        if (error) {
            *error = pn_error_init(PN_ERROR_DOMAIN, "NaN and infinity name no number a"
                                                    " tablet could hold");
        }
        return NULL;
    }
    if ((int) frac_places > PN_BASE60_MAX_PLACES) {
        if (error) {
            *error = pn_error_init(PN_ERROR_DOMAIN,
                                   "asked for %u fractional places;"
                                   " the value holds %u places total",
                                   (unsigned) frac_places, PN_BASE60_MAX_PLACES);
        }
        return NULL;
    }

    double mag = fabs(x);
    double ip = floor(mag); /* the whole part */
    double fp = mag - ip;   /* the fraction: from 0 up to, but not, 1 */

    /*  The whole-value places, LEAST significant first: ipd[0] holds
  the 60^0 place, the one the fraction leans against.  Divide
  with a correction step: ip / 60.0 rounds, so the remainder is
  worked out again instead of trusting the pair the division
  hands back.
*/
    uint8_t ipd[PN_BASE60_MAX_PLACES];
    size_t ki = 0;

    while (ip >= 1.0) {
        if (ki == PN_BASE60_MAX_PLACES) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "the whole part needs more than %u"
                                       " base-60 places",
                                       PN_BASE60_MAX_PLACES);
            }
            return NULL;
        }
        double q = floor(ip / 60.0);
        double r = ip - q * 60.0;

        if (r >= 60.0) {
            q += 1.0;
            r -= 60.0;
        }
        else if (r < 0.0) {
            q -= 1.0;
            r += 60.0;
        }
        ipd[ki++] = (uint8_t) r; /* r is a whole number from 0 to 59 */
        ip = q;
    }

    /*  The fractional places, MOST significant first: fpd[0] holds
  the 60^-1 place, made by the first multiply.  The last kept
  place rounds half away from zero; trailing zeros stay, so
  every answer is reproducible.
*/
    uint8_t fpd[PN_BASE60_MAX_PLACES];
    size_t kf = frac_places;

    for (size_t j = 0; j < kf; j++) {
        if (ki + j == PN_BASE60_MAX_PLACES) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "%u whole places plus this fraction"
                                       " will not share %u slots",
                                       (unsigned) ki, PN_BASE60_MAX_PLACES);
            }
            return NULL;
        }
        fp *= 60.0;
        uint8_t dgt = (uint8_t) fp; /* floor: fp cannot go below 0 */

        fp -= (double) dgt;
        if (j + 1 == kf && fp >= 0.5) {
            dgt++; /* may reach 60; the carry chain below fixes it */
        }
        fpd[j] = dgt;
    }

    /*  Fraction carries run right to left, every place handing its
  overflow to the neighbour on its left; whatever escapes past
  the radix point leaves a carry for the whole side.  With no
  fraction places kept at all, the leftover fraction itself
  decides the rounding.
*/
    int carry = (kf == 0 && fp >= 0.5);

    for (size_t i = kf; i-- > 0;) {
        if (fpd[i] == 60) {
            fpd[i] = 0;
            if (i > 0) {
                fpd[i - 1]++;
            }
            else {
                carry = 1;
            }
        }
    }

    /*  The whole-part carry chain, least to most significant; a carry
  that walks clean off the top becomes a new leading 1 - every
  place it crossed is zero already, carried clean through.
*/
    int carry_out = 0;

    if (carry) {
        size_t j = 0;

        for (;;) {
            if (j == ki) { /* carried past the most significant */
                carry_out = 1;
                break;
            }
            if (++ipd[j] != 60) {
                break;
            }
            ipd[j++] = 0;
        }
    }

    pn_base60 v;

    memset(&v, 0, sizeof v);
    v.neg = signbit(x) != 0;

    size_t total = ki + kf;

    if (carry_out) {
        if (total == PN_BASE60_MAX_PLACES) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "rounding grew the value past its"
                                       " %u places; there is no room for"
                                       " the new leading place",
                                       PN_BASE60_MAX_PLACES);
            }
            return NULL;
        }
        v.places[0] = 1;
        total++; /* all the other places stay zero */
        v.radix = (uint8_t) (ki + 1);
    }
    else {
        for (size_t i = 0; i < ki; i++) {
            v.places[i] = ipd[ki - 1 - i]; /* whole side flipped to MSB-first */
        }
        for (size_t i = 0; i < kf; i++) {
            v.places[ki + i] = fpd[i]; /* fraction is MSB-first already */
        }
        v.radix = (uint8_t) ki;
        if (total == 0) {
            total = 1; /* everything rounded to nought: {0} */
        }
    }

    v.n_places = (uint8_t) total;
    *out = v;
    return out;
}
