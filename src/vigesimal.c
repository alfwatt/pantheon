/*  vigesimal.c — libPantheon: base-twenty core.
 *
 *  Base-20 places, scholarly transcriptions, and conversion to and from
 *  uint64_t / double.  No allocation anywhere; the largest temporaries
 *  are a 16-byte place array and a 50-byte format scratch buffer.
 *
 *  The Maya wrote neither radix points nor signs; both are modern
 *  niceties, exactly as in pn_sex60.  The one substantive difference
 *  from sexagesimal is arithmetical: 20 = 4 x 5, so fractions with a
 *  factor five in the denominator are as little dyadic as sixtieths
 *   (0;1 = 1/20 repeats forever in binary), while 20^12 is the last
 *  power under 2^53 — a whole row more breathing room than 60^8, worth
 *  a note to anyone trusting pn_vigesimal_to_double as canonical.
 *
 *  References: https://en.wikipedia.org/wiki/Maya_numerals ,
 *  https://en.wikipedia.org/wiki/Vigesimal
 */

#include <math.h>
#include <string.h>

#include "pantheon.h"

/* Exact powers of 20 as integers: 20^0 .. 20^14.   20^14 = 1.6384e18 is
 * the last that fits a uint64_t (20^15 = 3.2768e19 would not); only up
 * to 20^12 = 4.096e15 does one still fit 2^53 and so stay exact as a
 * double. */
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

/* A value is well-formed when it has 1..MAX places, each <= 19, and a
 * radix between 0 and n_places.  A struct the caller poked at by hand
 * must not be able to make the other functions misbehave. */
static int well_formed(pn_vigesimal v)
{
	if (v.n_places == 0 || v.n_places > PN_VIGESIMAL_MAX_PLACES)
		return 0;
	if (v.radix > v.n_places)
		return 0;
	for (unsigned i = 0; i < v.n_places; i++)
		if (v.places[i] > 19)
			return 0;
	return 1;
}

/* --- parsing ------------------------------------------------------------- */

pn_status pn_vigesimal_parse(const char *text, pn_vigesimal *out)
{
	if (text == NULL || out == NULL)
		return pn_err_null;

	pn_vigesimal v; /* build locally; *out is untouched unless we succeed */
	memset(&v, 0, sizeof v);

	const char *p = text;
	if (*p == '-') {
		v.neg = true;
		p++;
	}

	int radix = -1; /* index of the ';' boundary, if any */
	for (;;) {
		/* One place: a run of one or two decimal digits.  A longer
		 * run (the "100" of a mistyped decimal) is read three
		 * digits deep and rejected as out of range. */
		unsigned d = 0;
		int nd = 0;
		while (nd < 3 && *p >= '0' && *p <= '9') {
			d = d * 10u + (unsigned)(*p - '0');
			p++;
			nd++;
		}
		if (nd == 0)
			return pn_err_syntax;     /* "", "-", "1;", " 1", junk */
		if (nd > 2 || d > 19u)
			return pn_err_range;
		if (v.n_places == PN_VIGESIMAL_MAX_PLACES)
			return pn_err_range;
		v.places[v.n_places++] = (uint8_t)d;

		if (*p == '\0')
			break;
		if (*p == ';') {
			if (radix >= 0)
				return pn_err_syntax; /* a second ';' */
			radix = v.n_places;        /* places left of the point */
		} else if (*p != ',' && *p != ':') {
			return pn_err_syntax;      /* any other non-digit */
		}
		p++;
	}

	v.radix = (uint8_t)(radix >= 0 ? radix : v.n_places);
	*out = v;
	return pn_ok;
}

/* --- formatting ----------------------------------------------------------- */

pn_status pn_vigesimal_format(pn_vigesimal v, char sep, char *buf, size_t buflen)
{
	if (buf == NULL)
		return pn_err_null;
	if (sep != ',' && sep != ':' && sep != ';')
		return pn_err_syntax;
	if (!well_formed(v))
		return pn_err_domain;

	/* Build in scratch space so a failure cannot leave buf half-written.
	  * Worst case: '-' + 16 places x 2 digits + 15 separators + NUL = 49
	  * bytes, which the declared maximum accommodates.
	  *
	  * One deliberate departure from pn_sex60_format: places print with
	  * the FEWEST decimal digits they need, no zero padding past the
	  * first place.  Sexagesimal pads 0..59 places to two digits as the
	  * scholarly convention (1,24,51,10); a base-twenty place is 0..19
	  * and "02" would dress a codex digit in Babylonian clothes.  The
	  * first place still keeps its literal 0 — "0;5" means 1/4. */
	char tmp[PN_VIGESIMAL_FORMAT_MAX_LEN];
	char *q = tmp;
	if (v.neg)
			*q++ = '-';
	for (unsigned i = 0; i < v.n_places; i++) {
		if (i > 0)
				*q++ = (i == v.radix && v.radix < v.n_places) ? ';' : sep;
		unsigned d = v.places[i];
		if (d >= 10u)                        /* two digits when it takes two */
				*q++ = (char)('0' + (int)(d / 10u));
		*q++ = (char)('0' + (int)(d % 10u));
	}

	size_t len = (size_t)(q - tmp);
	if (len + 1 > buflen)
		return pn_err_buffer;
	memcpy(buf, tmp, len);
	buf[len] = '\0';
	return pn_ok;
}

/* --- integer conversion ----------------------------------------------------
 *
 * from_u64 peels places off the bottom (repeated division by 20); to_u64
 * reassembles with Horner's scheme.  The codices did exactly this on the
 * counting surface — the Dresden page arithmetic — and there is no
 * Babylonian-style missing-zero tragedy here: the Maya had the shell.
 * --------------------------------------------------------------------------- */

pn_status pn_vigesimal_from_u64(uint64_t n, pn_vigesimal *out)
{
	if (out == NULL)
		return pn_err_null;

	pn_vigesimal v;
	memset(&v, 0, sizeof v);
	v.neg = false;

	uint8_t *p = v.places; /* fills least-significant first... */
	size_t k = 0;
	do {
		if (k == PN_VIGESIMAL_MAX_PLACES)
			return pn_err_range; /* unreachable: 2^64 takes 15 places */
		p[k++] = (uint8_t)(n % 20u);
		n /= 20u;
	} while (n != 0);
	/* ...then reversed in place */
	for (size_t i = 0; i < k / 2; i++) {
		uint8_t t = p[i];
		p[i] = p[k - 1 - i];
		p[k - 1 - i] = t;
	}
	v.n_places = (uint8_t)k;
	v.radix = (uint8_t)k;
	*out = v;
	return pn_ok;
}

pn_status pn_vigesimal_to_u64(pn_vigesimal v, uint64_t *out)
{
	if (out == NULL)
		return pn_err_null;
	if (v.n_places == 0 || v.n_places > PN_VIGESIMAL_MAX_PLACES ||
			v.radix > v.n_places)
		return pn_err_domain;
	if (v.radix != v.n_places)
		return pn_err_domain; /* a fractional place: not an integer */

	uint64_t acc = 0;
	for (unsigned i = 0; i < v.n_places; i++) {
		uint64_t d = v.places[i];
		if (d > 19u)
			return pn_err_domain; /* well_formed also rejects this */
		if (acc > (UINT64_MAX - d) / 20u)
			return pn_err_range;
		acc = acc * 20u + d;
	}
	*out = acc;               /* the magnitude: neg is the caller's */
	                          /* business, exactly as pn_sex60_to_u64  */
	return pn_ok;
}

/* --- double conversion ------------------------------------------------------
 *
 * to_double computes the integer part and the fractional part each as an
 * exact uint64_t where one fits, then performs a single correctly-rounded
 * division — so results are exact whenever a double can hold them at all
 * (and in base twenty "at all" reaches 20^12's worth of integer places).
 * --------------------------------------------------------------------------- */

pn_status pn_vigesimal_to_double(pn_vigesimal v, double *out)
{
	if (out == NULL)
		return pn_err_null;
	if (!well_formed(v))
		return pn_err_domain;

	double val = 0.0;

	/* integer part: Horner, exact up to uint64 range */
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
	val = (double)acc;
	if (overflowed)                  /* beyond uint64: finish in double */
		for (; i < v.radix; i++)
			val = val * 20.0 + (double)v.places[i];

	/* fractional part: one exact integer F over one exact power 20^k,
	 * i.e. a single correctly-rounded division while k <= 14. */
	unsigned k = (unsigned)v.n_places - v.radix;
	if (k > 0) {
		if (k < POW20_COUNT) {
			uint64_t f = 0;
			for (i = v.radix; i < v.n_places; i++)
				f = f * 20u + v.places[i];
			val += (double)f / (double)pow20_exact[k];
		} else {                     /* too deep for exact integers */
			double frac = 0.0;
			for (i = v.n_places; i-- > v.radix; )
				frac = (frac + (double)v.places[i]) / 20.0;
			val += frac;
		}
	}

	*out = v.neg ? -val : val;
	return pn_ok;
}

pn_status pn_vigesimal_from_double(double x, uint8_t frac_places, pn_vigesimal *out)
{
	if (out == NULL)
		return pn_err_null;
	if (isnan(x) || isinf(x) || (int)frac_places > PN_VIGESIMAL_MAX_PLACES)
		return pn_err_domain;

	double mag = fabs(x);
	double ip = floor(mag);            /* integer part */
	double fp = mag - ip;              /* fraction in [0, 1) */

	/* Integer places, LEAST significant first: ipd[0] is the 20^0 place,
	 * the one the fraction sits against.  Division with a correction
	 * step: ip / 20.0 rounds, so re-derive the remainder rather than
	 * trusting the quotient's partner. */
	uint8_t ipd[PN_VIGESIMAL_MAX_PLACES];
	size_t ki = 0;
	while (ip >= 1.0) {
		if (ki == PN_VIGESIMAL_MAX_PLACES)
			return pn_err_range;
		double q = floor(ip / 20.0);
		double r = ip - q * 20.0;
		if (r >= 20.0) {
			q += 1.0;
			r -= 20.0;
		} else if (r < 0.0) {
			q -= 1.0;
			r += 20.0;
		}
		ipd[ki++] = (uint8_t)r;        /* r is an exact integer in [0, 20) */
		ip = q;
	}

	/* Fractional places, MOST significant first: fpd[0] is the 20^-1
	 * place, produced by the first multiply.  Rounded half-away-from-
	 * zero at the last kept place; trailing zeros are kept so results
	 * are deterministic. */
	uint8_t fpd[PN_VIGESIMAL_MAX_PLACES];
	size_t kf = frac_places;
	for (size_t j = 0; j < kf; j++) {
		if (ki + j == PN_VIGESIMAL_MAX_PLACES)
			return pn_err_range;
		fp *= 20.0;
		uint8_t dgt = (uint8_t)fp;           /* floor: fp is non-negative */
		fp -= (double)dgt;
		if (j + 1 == kf && fp >= 0.5)
			dgt++;                           /* may reach 20; carries fix it */
		fpd[j] = dgt;
	}
	/* Fraction carries run right to left (each place feeds its more
	 * significant neighbour); what survives past the radix point leaves
	 * a carry for the integer side.  With no places kept at all, the
	 * surviving fraction itself decides the rounding. */
	int carry = (kf == 0 && fp >= 0.5);
	for (size_t i = kf; i-- > 0; )
		if (fpd[i] == 20) {
			fpd[i] = 0;
			if (i > 0)
				fpd[i - 1]++;
			else
				carry = 1;
		}

	/* Integer carry chain, least to most significant; a carry clean
	 * through the top becomes a new leading 1 — every place it passed
	 * is already zero, carried clean through. */
	int carry_out = 0;
	if (carry) {
		size_t j = 0;
		for (;;) {
			if (j == ki) {            /* carried past the most significant */
				carry_out = 1;
				break;
			}
			if (++ipd[j] != 20)
				break;
			ipd[j++] = 0;
		}
	}

	pn_vigesimal v;
	memset(&v, 0, sizeof v);
	v.neg = signbit(x) != 0;

	size_t total = ki + kf;
	if (carry_out) {
		if (total == PN_VIGESIMAL_MAX_PLACES)
			return pn_err_range;      /* no room for the new leading place */
		v.places[0] = 1;
		total++;                      /* all other places stay zero */
		v.radix = (uint8_t)(ki + 1);
	} else {
		for (size_t i = 0; i < ki; i++)
			v.places[i] = ipd[ki - 1 - i];   /* integers reversed to MSB-first */
		for (size_t i = 0; i < kf; i++)
			v.places[ki + i] = fpd[i];       /* fractions already MSB-first */
		v.radix = (uint8_t)ki;
		if (total == 0)
			total = 1;                /* everything rounded to nought: {0} */
	}

	v.n_places = (uint8_t)total;
	*out = v;
	return pn_ok;
}
