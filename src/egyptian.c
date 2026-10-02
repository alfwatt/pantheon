/*  egyptian.c — libPantheon: hieroglyphic numerals, UTF-8.
   *
   *  Seven signs do all the work, one per power of ten, each repeated as
   *  many times as its digit demands — a decimal system with no position
   *  at all:
   *
   *    U+133E4 EGYPTIAN HIEROGLYPH Z001   (single stroke)    = 1
   *    U+13386 EGYPTIAN HIEROGLYPH V020   (cattle hobble)    = 10
   *    U+13362 EGYPTIAN HIEROGLYPH V001   (coil of rope)     = 100
   *    U+131BC EGYPTIAN HIEROGLYPH M012   (lotus)            = 1 000
   *    U+130AD EGYPTIAN HIEROGLYPH D050   (bent finger)      = 10 000
   *    U+13190 EGYPTIAN HIEROGLYPH I008   (tadpole)          = 100 000
   *    U+13068 EGYPTIAN HIEROGLYPH C011   (Heh god)          = 1 000 000
   *
   *  Runs are written in descending powers with no separators — the
   *  distinct signs are self-delimiting, and unlike the cuneiform gap
   *  there is no blank between places to preserve.  There is no sign
   *  above the Heh and no zero: the scribe wrote a blank for nothing,
   *  so 0 renders as the empty string and anything past nine of each
   *  sign is out of range rather than absurdly long.
   *
   *  The signs are written as universal character names so this source
   *  stays pure ASCII.
   *
   *  Reference: https://en.wikipedia.org/wiki/Egyptian_numerals
   */

#include <string.h>

#include "pantheon.h"

#define STROKE_UTF8   "\U000133E4" /* UTF-8: F0 93 8F A4 */
#define HOBBLE_UTF8   "\U00013386" /* UTF-8: F0 93 8E 86 */
#define COIL_UTF8     "\U00013362" /* UTF-8: F0 93 8D A2 */
#define LOTUS_UTF8    "\U000131BC" /* UTF-8: F0 93 86 BC */
#define FINGER_UTF8   "\U000130AD" /* UTF-8: F0 93 82 AD */
#define TADPOLE_UTF8  "\U00013190" /* UTF-8: F0 93 86 90 */
#define HEH_UTF8      "\U00013068" /* UTF-8: F0 93 81 A8 */
#define SIGN_BYTES 4               /* all seven signs are 4 bytes of UTF-8 */

#define N_SIGNS 7 /* the powers 10^0 .. 10^6; there is no sign above Heh */

/* Descending: Heh first, stroke last, as on the temple wall. */
static const char *const signs[N_SIGNS] = {
	HEH_UTF8, TADPOLE_UTF8, FINGER_UTF8, LOTUS_UTF8,
	COIL_UTF8, HOBBLE_UTF8, STROKE_UTF8
};

/* The value each sign above is worth. */
static const uint64_t values[N_SIGNS] = {
	1000000u, 100000u, 10000u, 1000u, 100u, 10u, 1u
};

size_t pn_egyptian_render_size(uint64_t n)
{
	/* Pure and total.  For n <= PN_EGYPTIAN_MAX_VALUE this is exactly
	 * 4 * (sum of decimal digits); larger n keeps counting digit copies
	 * as if every digit past 10^6 could still be drawn and the answer
	 * is simply meaningless — render rejects such n with pn_err_range. */
	size_t bytes = 0;
	while (n != 0) {
		bytes += (size_t)(n % 10u) * SIGN_BYTES;
		n /= 10u;
	}
	return bytes;
}

pn_status pn_egyptian_render(uint64_t n, char *buf, size_t buflen)
{
	if (buf == NULL)
		return pn_err_null;
	if (n > PN_EGYPTIAN_MAX_VALUE)
		return pn_err_range;  /* no sign above the Heh: ten Heh are not written here */

	size_t need = pn_egyptian_render_size(n); /* exact: buflen must fit it + NUL */
	if (buflen <= need)
		return pn_err_buffer;

	char *q = buf; /* safe: need is exact and validated against buflen */
	for (unsigned i = 0; i < N_SIGNS; i++) {
		uint64_t digit = (n / values[i]) % 10u;
		for (uint64_t j = 0; j < digit; j++) {
			memcpy(q, signs[i], SIGN_BYTES);
			q += SIGN_BYTES;
		}
	}
	*q = '\0';                  /* n == 0: just the blank, and the NUL */
	return pn_ok;
}

/* --- parsing ------------------------------------------------------------
 *
 * Canonical form is what makes the round trip exact: the seven signs,
 * grouped into one contiguous run each, the runs strictly descending in
 * power, every run 1..9 copies.  Anything else — foreign bytes, a
 * cut-off sign, a run written bottom-up or split in two — is rejected
 * the way a modern reader rejects "IIIX".  Ten copies of one sign are
 * not bad grammar but bad arithmetic: the scribe would have carried to
 * the next power, so that string names no canonical number (range, not
 * syntax — the same ruling six U wedges get in pn_babylonian_parse).
 */

pn_status pn_egyptian_parse(const char *buf, size_t buflen, uint64_t *out)
{
	if (buf == NULL || out == NULL)
		return pn_err_null;

	uint64_t sum = 0;      /* build locally; *out untouched unless we succeed */
	int prev = -1;         /* index of the previous run's sign (runs descend) */
	const char *p = buf, *end = buf + buflen;

	while (p < end) {
		int found = -1;
		for (int i = 0; i < N_SIGNS; i++)
			if (p + SIGN_BYTES <= end && memcmp(p, signs[i], SIGN_BYTES) == 0) {
				found = i;
				break;
			}
		if (found < 0)
			return pn_err_syntax;  /* foreign byte, or a sign cut off short */
		if (prev >= 0 && found <= prev)
			return pn_err_syntax;  /* ascending order, or a second run of one sign */

		size_t count = 0;
		do {
			p += SIGN_BYTES;
			if (++count > 9u)
				return pn_err_range;  /* ten strokes: carry to the hobble, scribe */
		} while (p + SIGN_BYTES <= end &&
				memcmp(p, signs[found], SIGN_BYTES) == 0);

		/* No overflow: count <= 9 over distinct descending signs caps the
		 * sum at 9 * (10^6 + ... + 1) = 9 999 999 = PN_EGYPTIAN_MAX_VALUE. */
		sum += count * values[found];
		prev = found;
	}

	/* An empty buffer is the scribe's blank: it reads back as 0. */
	*out = sum;
	return pn_ok;
}
