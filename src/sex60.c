/*  sex60.c — libPantheon: the Babylonian base-60 core.

    Base-60 places, the scholarly way of writing them, and the roads
    to and from uint64_t and double.  Success allocates nothing: the
    biggest temporaries here are a 16-byte place array and a 50-byte
    scratch buffer.

    References: https://en.wikipedia.org/wiki/Sexagesimal ,
    https://en.wikipedia.org/wiki/Babylonian_numerals
*/
#include <math.h>
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
static int well_formed(pn_sex60 v)
{
	if (v.n_places == 0 || v.n_places > PN_SEX60_MAX_PLACES)
		return 0;
	if (v.radix > v.n_places)
		return 0;
	for (unsigned i = 0; i < v.n_places; i++)
		if (v.places[i] > 59)
			return 0;
	return 1;
}

bool pn_sex60_parse(const char *text, pn_sex60 *_Nullable out,
                    pn_error *_Nullable *_Nullable error)
{
	if (text == NULL || out == NULL)
		return pn_error_set(error, PN_ERROR_NULL,
			                 "parse needs a string and a place to put"
			                 " the value");

	pn_sex60 v; /* built here; *out keeps its old value unless all works */
	memset(&v, 0, sizeof v);

	const char *p = text;
	if (*p == '-') {
		v.neg = true;
		p++;
	}

	int radix = -1; /* where the ';' sits, if it sits anywhere */
	for (;;) {
		 /* One place: one or two decimal digits.  A longer run of
		  * digits — the "3599" from the header docs — is read
		  * three digits deep and refused as out of range.        */
		unsigned d = 0;
		int nd = 0;

		while (nd < 3 && *p >= '0' && *p <= '9') {
			d = d * 10u + (unsigned)(*p - '0');
			p++;
			nd++;
		}
		if (nd == 0)
			return pn_error_set(error, PN_ERROR_SYNTAX,
				                "a place needs one or two digits;"
				                " found none at byte %zu of the"
				                " string",
				                (size_t)(p - text));
		if (nd > 2 || d > 59u)
			return pn_error_set(error, PN_ERROR_RANGE,
				                "place %u reads %u: a place holds 0"
				                " to 59, written with 1 to 2 digits"
				                " \"%.*s...\" included %s",
				                v.n_places, d, nd > 3 ? 3 : nd,
				                p - nd,
				                nd > 2 ? "(three or more digits)"
				                        : "(a place above 59)");
		if (v.n_places == PN_SEX60_MAX_PLACES)
			return pn_error_set(error, PN_ERROR_RANGE,
				                "a value holds at most %u places"
				                " (60^%u runs past what the type"
				                " stores)",
				                PN_SEX60_MAX_PLACES,
				                PN_SEX60_MAX_PLACES);

		v.places[v.n_places++] = (uint8_t)d;

		if (*p == '\0')
			break;
		if (*p == ';') {
			if (radix >= 0)
				return pn_error_set(error, PN_ERROR_SYNTAX,
				                      "a second ';' at byte %zu:"
				                      " the string gets one radix"
				                      " point at most",
				                      (size_t)(p - text));
			radix = v.n_places;   /* places left of the point */
		} else if (*p != ',' && *p != ':') {
			return pn_error_set(error, PN_ERROR_SYNTAX,
			                     "byte %zu of the string must join"
			                     " places with ',' ':' or ';' or"
			                     " end the value",
			                     (size_t)(p - text));
		}
		p++;
	}

	v.radix = (uint8_t)(radix >= 0 ? radix : v.n_places);
	*out = v;
	return true;
}

bool pn_sex60_format(pn_sex60 v, char sep, char *_Nonnull buf,
                     size_t buflen, pn_error *_Nullable *_Nullable error)
{
	if (buf == NULL)
		return pn_error_set(error, PN_ERROR_NULL,
				                "format needs a buffer to write into");
	if (sep != ',' && sep != ':' && sep != ';')
		return pn_error_set(error, PN_ERROR_SYNTAX,
				                "places may be joined with ',' ':'"
				                " or ';' only, not '%c'", sep);
	if (!well_formed(v))
		return pn_error_set(error, PN_ERROR_DOMAIN,
				                "the value is not well-formed:"
				                " %u places, radix %u — places need"
				                " 1 to %u slots, each 0 to 59, and"
				                " the radix cannot pass n_places",
				                v.n_places, v.radix,
				                PN_SEX60_MAX_PLACES);

	/* Written in scratch space first, so a late failure can never
	 * leave buf holding half a number.  Worst case: '-' + 16
	 * places x 2 digits + 15 separators + NUL = 49 bytes, which
	 * the declared maximum has room for. */
	char tmp[PN_SEX60_FORMAT_MAX_LEN];
	char *q = tmp;

	if (v.neg)
		*q++ = '-';
	for (unsigned i = 0; i < v.n_places; i++) {
		if (i > 0)
			*q++ = (i == v.radix && v.radix < v.n_places) ? ';' : sep;
		unsigned d = v.places[i];

		if (i > 0 || d >= 10)   /* the first place drops its leading zero */
			*q++ = (char)('0' + (int)(d / 10u));
		*q++ = (char)('0' + (int)(d % 10u));
	}

	size_t len = (size_t)(q - tmp);

	if (len + 1 > buflen)
		return pn_error_set(error, PN_ERROR_BUFFER,
				                "buffer holds %zu bytes; this value"
				                " needs %zu plus the terminating"
				                " NUL",
				                buflen, len);
	memcpy(buf, tmp, len);
	buf[len] = '\0';
	return true;
}

/* from_u64 peels places off the bottom (divide by 60 again and
   again); to_u64 builds the number back up with Horner's scheme.
   Babylonian clerks did exactly this on the abacus, and exactly this
   in the wrong order whenever we misread a tablet's missing zero. */
bool pn_sex60_from_u64(uint64_t n, pn_sex60 *_Nullable out,
                       pn_error *_Nullable *_Nullable error)
{
	if (out == NULL)
		return pn_error_set(error, PN_ERROR_NULL,
				                "from_u64 needs a place to put the"
				                " value");

	pn_sex60 v;

	memset(&v, 0, sizeof v);
	v.neg = false;

	uint8_t *p = v.places; /* filled least-significant first... */
	size_t k = 0;

	do {
		if (k == PN_SEX60_MAX_PLACES)
			return pn_error_set(error, PN_ERROR_RANGE,
				                "this many base-60 places will not"
				                " fit the value (the guard reads as"
				                " unreachable: 2^64 needs 11 places,"
				                " the type holds %u)",
				                PN_SEX60_MAX_PLACES);
		p[k++] = (uint8_t)(n % 60u);
		n /= 60u;
	} while (n != 0);
	/* ...then flipped around in place */
	for (size_t i = 0; i < k / 2; i++) {
		uint8_t t = p[i];

		p[i] = p[k - 1 - i];
		p[k - 1 - i] = t;
	}
	v.n_places = (uint8_t)k;
	v.radix = (uint8_t)k;
	*out = v;
	return true;
}

bool pn_sex60_to_u64(pn_sex60 v, uint64_t *_Nullable out,
                     pn_error *_Nullable *_Nullable error)
{
	if (out == NULL)
		return pn_error_set(error, PN_ERROR_NULL,
				                "to_u64 needs a place to put the"
				                " number");
	if (v.n_places == 0 || v.n_places > PN_SEX60_MAX_PLACES ||
	    v.radix > v.n_places)
		return pn_error_set(error, PN_ERROR_DOMAIN,
				                "the value is not well-formed:"
				                " %u places, radix %u",
				                v.n_places, v.radix);
	if (v.radix != v.n_places)
		return pn_error_set(error, PN_ERROR_DOMAIN,
				                "the value has %u fractional place(s)"
				                " after the radix point, so it is no"
				                " whole number",
				                (unsigned)v.n_places - v.radix);

	uint64_t acc = 0;

	for (unsigned i = 0; i < v.n_places; i++) {
		uint64_t d = v.places[i];

		if (d > 59u)
			return pn_error_set(error, PN_ERROR_DOMAIN,
				                "place %u holds %u: a place runs 0"
				                " to 59", i, (unsigned)d);
		if (acc > (UINT64_MAX - d) / 60u)
			return pn_error_set(error, PN_ERROR_RANGE,
				                "the value needs %u base-60 places:"
				                " more than a uint64_t can count",
				                v.n_places);
		acc = acc * 60u + d;
	}
	*out = acc;
	return true;
}

/* to_double works out the whole part and the fraction each as an
   exact uint64_t where one fits, then does one correctly-rounded
   division.  So the answer is exact whenever a double is able to
   hold it at all. */
bool pn_sex60_to_double(pn_sex60 v, double *_Nullable out,
                        pn_error *_Nullable *_Nullable error)
{
	if (out == NULL)
		return pn_error_set(error, PN_ERROR_NULL,
				                "to_double needs a place to put the"
				                " number");
	if (!well_formed(v))
		return pn_error_set(error, PN_ERROR_DOMAIN,
				                "the value is not well-formed:"
				                " %u places, radix %u",
				                v.n_places, v.radix);

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
	val = (double)acc;
	if (overflowed)   /* past uint64 territory: carry on in double */
		for (; i < v.radix; i++)
			val = val * 60.0 + (double)v.places[i];

	/* the fraction: one exact whole number F over one exact power
	 * 60^k, which is a single correctly-rounded division while
	 * k <= 10. */
	unsigned k = (unsigned)v.n_places - v.radix;

	if (k > 0) {
		if (k < POW60_COUNT) {
			uint64_t f = 0;

			for (i = v.radix; i < v.n_places; i++)
				f = f * 60u + v.places[i];
			val += (double)f / (double)pow60_exact[k];
		} else {         /* too deep down for exact integers */
			double frac = 0.0;

			for (i = v.n_places; i-- > v.radix; )
				frac = (frac + (double)v.places[i]) / 60.0;
			val += frac;
		}
	}

	*out = v.neg ? -val : val;
	return true;
}

bool pn_sex60_from_double(double x, uint8_t frac_places,
                          pn_sex60 *_Nullable out,
                          pn_error *_Nullable *_Nullable error)
{
	if (out == NULL)
		return pn_error_set(error, PN_ERROR_NULL,
				                "from_double needs a place to put the"
				                " value");
	if (isnan(x) || isinf(x))
		return pn_error_set(error, PN_ERROR_DOMAIN,
				                "NaN and infinity name no number a"
				                " tablet could hold");
	if ((int)frac_places > PN_SEX60_MAX_PLACES)
		return pn_error_set(error, PN_ERROR_DOMAIN,
				                "asked for %u fractional places;"
				                " the value holds %u places total",
				                (unsigned)frac_places,
				                PN_SEX60_MAX_PLACES);

	double mag = fabs(x);
	double ip = floor(mag);   /* the whole part */
	double fp = mag - ip;     /* the fraction: from 0 up to, but not, 1 */

	/* The whole-value places, LEAST significant first: ipd[0] holds
	 * the 60^0 place, the one the fraction leans against.  Divide
	 * with a correction step: ip / 60.0 rounds, so the remainder is
	 * worked out again instead of trusting the pair the division
	 * hands back. */
	uint8_t ipd[PN_SEX60_MAX_PLACES];
	size_t ki = 0;

	while (ip >= 1.0) {
		if (ki == PN_SEX60_MAX_PLACES)
			return pn_error_set(error, PN_ERROR_RANGE,
				                "the whole part needs more than %u"
				                " base-60 places",
				                PN_SEX60_MAX_PLACES);
		double q = floor(ip / 60.0);
		double r = ip - q * 60.0;

		if (r >= 60.0) {
			q += 1.0;
			r -= 60.0;
		} else if (r < 0.0) {
			q -= 1.0;
			r += 60.0;
		}
		ipd[ki++] = (uint8_t)r;  /* r is a whole number from 0 to 59 */
		ip = q;
	}

	/* The fractional places, MOST significant first: fpd[0] holds
	 * the 60^-1 place, made by the first multiply.  The last kept
	 * place rounds half away from zero; trailing zeros stay, so
	 * every answer is reproducible. */
	uint8_t fpd[PN_SEX60_MAX_PLACES];
	size_t kf = frac_places;

	for (size_t j = 0; j < kf; j++) {
		if (ki + j == PN_SEX60_MAX_PLACES)
			return pn_error_set(error, PN_ERROR_RANGE,
				                "%u whole places plus this fraction"
				                " will not share %u slots",
				                (unsigned)ki, PN_SEX60_MAX_PLACES);
		fp *= 60.0;
		uint8_t dgt = (uint8_t)fp;        /* floor: fp cannot go below 0 */

		fp -= (double)dgt;
		if (j + 1 == kf && fp >= 0.5)
			dgt++;     /* may reach 60; the carry chain below fixes it */
		fpd[j] = dgt;
	}

	/* Fraction carries run right to left, every place handing its
	 * overflow to the neighbour on its left; whatever escapes past
	 * the radix point leaves a carry for the whole side.  With no
	 * fraction places kept at all, the leftover fraction itself
	 * decides the rounding. */
	int carry = (kf == 0 && fp >= 0.5);

	for (size_t i = kf; i-- > 0; )
		if (fpd[i] == 60) {
			fpd[i] = 0;
			if (i > 0)
				fpd[i - 1]++;
			else
				carry = 1;
			}

	/* The whole-part carry chain, least to most significant; a carry
	 * that walks clean off the top becomes a new leading 1 — every
	 * place it crossed is zero already, carried clean through. */
	int carry_out = 0;

	if (carry) {
		size_t j = 0;

		for (;;) {
			if (j == ki) {     /* carried past the most significant */
				carry_out = 1;
				break;
			}
			if (++ipd[j] != 60)
				break;
			ipd[j++] = 0;
		}
	}

	pn_sex60 v;

	memset(&v, 0, sizeof v);
	v.neg = signbit(x) != 0;

	size_t total = ki + kf;

	if (carry_out) {
		if (total == PN_SEX60_MAX_PLACES)
			return pn_error_set(error, PN_ERROR_RANGE,
				                "rounding grew the value past its"
				                " %u places; there is no room for"
				                " the new leading place",
				                PN_SEX60_MAX_PLACES);
		v.places[0] = 1;
		total++;             /* all the other places stay zero */
		v.radix = (uint8_t)(ki + 1);
	} else {
		for (size_t i = 0; i < ki; i++)
			v.places[i] = ipd[ki - 1 - i];  /* whole side flipped to MSB-first */
		for (size_t i = 0; i < kf; i++)
			v.places[ki + i] = fpd[i];      /* fraction is MSB-first already */
		v.radix = (uint8_t)ki;
		if (total == 0)
			total = 1;     /* everything rounded to nought: {0} */
	}

	v.n_places = (uint8_t)total;
	*out = v;
	return true;
}
