/*  egyptian.c — libPantheon: hieroglyphic numerals, UTF-8.

    Seven signs do all the work, one per power of ten, each repeated
    as many times as its digit calls for.  It is a decimal system
    with no place values at all.

    U+133E4 EGYPTIAN HIEROGLYPH Z001  (single stroke)   = 1
    U+13386 EGYPTIAN HIEROGLYPH V020  (cattle hobble)   = 10
    U+13362 EGYPTIAN HIEROGLYPH V001  (coil of rope)    = 100
    U+131BC EGYPTIAN HIEROGLYPH M012  (lotus)           = 1 000
    U+130AD EGYPTIAN HIEROGLYPH D050  (bent finger)     = 10 000
    U+13190 EGYPTIAN HIEROGLYPH I008  (tadpole)         = 100 000
    U+13068 EGYPTIAN HIEROGLYPH C011  (Heh god)         = 1 000 000

    Signs are written in runs from the biggest power down, with no
    separators between them: each sign looks different, so a reader
    always knows where one run stops and the next starts.  There is
    no sign above the Heh and no sign for zero: the scribe simply
    left a blank for nothing.  So 0 renders as the empty string, and
    any value larger than nine of every sign is out of range rather
    than absurdly long.

    The signs are spelled as universal character names so this file
    stays pure ASCII.

    Reference: https://en.wikipedia.org/wiki/Egyptian_numerals
*/
#include <string.h>

#include "pantheon.h"

#define STROKE_UTF8   "\U000133E4" /* bytes: F0 93 8F A4 */
#define HOBBLE_UTF8   "\U00013386" /* bytes: F0 93 8E 86 */
#define COIL_UTF8     "\U00013362" /* bytes: F0 93 8D A2 */
#define LOTUS_UTF8    "\U000131BC" /* bytes: F0 93 86 BC */
#define FINGER_UTF8   "\U000130AD" /* bytes: F0 93 82 AD */
#define TADPOLE_UTF8  "\U00013190" /* bytes: F0 93 86 90 */
#define HEH_UTF8      "\U00013068" /* bytes: F0 93 81 A8 */
#define SIGN_BYTES 4               /* every sign is 4 UTF-8 bytes */

#define N_SIGNS 7 /* powers 10^0 through 10^6; nothing above the Heh */

/* Biggest sign first, stroke last, as carved on the temple wall. */
static const char *const signs[N_SIGNS] = {
	HEH_UTF8, TADPOLE_UTF8, FINGER_UTF8, LOTUS_UTF8,
	COIL_UTF8, HOBBLE_UTF8, STROKE_UTF8
};

/* What each sign one row above is worth. */
static const uint64_t values[N_SIGNS] = {
	1000000u, 100000u, 10000u, 1000u, 100u, 10u, 1u
};

size_t pn_egyptian_render_size(uint64_t n)
{
	/* This answer never fails.  For n up to PN_EGYPTIAN_MAX_VALUE it
	 * is exactly 4 times the sum of the decimal digits.  Past that
	 * it keeps counting copies as if every digit above 10^6 could
	 * still be carved, so the answer is only meaningless — render
	 * turns down such n with PN_ERROR_RANGE anyway. */
	size_t bytes = 0;

	while (n != 0) {
		bytes += (size_t)(n % 10u) * SIGN_BYTES;
		n /= 10u;
	}
	return bytes;
}

bool pn_egyptian_render(uint64_t n, char *_Nonnull buf, size_t buflen,
                        pn_error *_Nullable *_Nullable error)
{
	if (buf == NULL)
		return pn_error_set(error, PN_ERROR_NULL,
			                "render needs a buffer to write into");
	if (n > PN_EGYPTIAN_MAX_VALUE) {
		/* There is no sign above the Heh, so ten Heh signs are
		 * not something this module writes. */
		return pn_error_set(error, PN_ERROR_RANGE,
			                "%llu needs ten or more signs of one kind;"
			                " the biggest sign is the Heh at 1000000",
			                (unsigned long long)n);
	}

	/* render_size is exact: buflen has to hold its bytes plus a NUL */
	size_t need = pn_egyptian_render_size(n);
	if (buflen <= need)
		return pn_error_set(error, PN_ERROR_BUFFER,
			                "buffer holds %zu bytes; %llu needs %zu plus"
			                " the terminating NUL",
			                buflen, (unsigned long long)n, need);

	char *q = buf; /* safe: need is exact and already checked */
	for (unsigned i = 0; i < N_SIGNS; i++) {
		uint64_t digit = (n / values[i]) % 10u;

		for (uint64_t j = 0; j < digit; j++) {
			memcpy(q, signs[i], SIGN_BYTES);
			q += SIGN_BYTES;
		}
	}
	*q = '\0'; /* n == 0 lands here: the blank, then the NUL */
	return true;
}

/* Parsing reads back what rendering wrote.

    Only the canonical spelling round-trips: the seven signs grouped
    into one run each, the runs stepping down in power, every run
    holding 1 to 9 copies.  Anything else — a byte that is not a
    sign, a sign sliced off short, a run built from the bottom up, or
    one sign written in two places — is rejected the way a reader
    today rejects "IIIX".  Ten copies of one sign is not bad spelling
    but bad arithmetic: the scribe would have carried to the next
    power, so that string names no canonical number.  That reply is
    PN_ERROR_RANGE, not PN_ERROR_SYNTAX — the same reply six U
    wedges get from pn_babylonian_parse. */
bool pn_egyptian_parse(const char *_Nullable buf, size_t buflen,
                       uint64_t *_Nullable out,
                       pn_error *_Nullable *_Nullable error)
{
	if (buf == NULL || out == NULL)
		return pn_error_set(error, PN_ERROR_NULL,
			                "parse needs a string and a place to put"
			                " the number");

	uint64_t sum = 0;   /* built here: *out stays untouched unless all works */
	int prev = -1;      /* which sign the previous run used; runs step down */
	const char *p = buf, *end = buf + buflen;

	while (p < end) {
		int found = -1;

		for (int i = 0; i < N_SIGNS; i++)
			if (p + SIGN_BYTES <= end &&
			    memcmp(p, signs[i], SIGN_BYTES) == 0) {
				found = i;
				break;
			}
		if (found < 0) {
			/* a byte no sign starts with, or a sign with
			 * bytes cut off its end */
			size_t room = (size_t)(end - p);

			return pn_error_set(error, PN_ERROR_SYNTAX,
			                    "byte at position %zu of %zu does not"
			                    " start a hieroglyphic sign (%zu bytes"
			                    " left there: not enough)"
			                    "%s",
			                    (size_t)(p - buf), buflen, room,
			                    room < SIGN_BYTES ? " — a sign is 4"
			                    " bytes whole" : "");
		}
		if (prev >= 0 && found <= prev)
			return pn_error_set(error, PN_ERROR_SYNTAX,
			                    "the sign at position %zu does not step"
			                    " down in power: runs must go from the"
			                    " Heh toward the stroke, and one sign"
			                    " gets a single run",
			                    (size_t)(p - buf));

		size_t count = 0;
		do {
			p += SIGN_BYTES;
			if (++count > 9u)
				return pn_error_set(error, PN_ERROR_RANGE,
				                    "ten copies of one sign at"
				                    " position %zu: ten strokes are"
				                    " written as one hobble, scribe"
				                    " — a run holds 1 to 9",
				                    (size_t)(p - buf) - SIGN_BYTES);
		} while (p + SIGN_BYTES <= end &&
				memcmp(p, signs[found], SIGN_BYTES) == 0);

		/* Nothing can overflow here: at most 9 copies over seven
		 * different descending signs, the sum stops at
		 * 9 * (10^6 + ... + 1) = 9 999 999, the domain's top. */
		sum += count * values[found];
		prev = found;
	}

	/* An empty string is the scribe's blank: it reads back as 0. */
	*out = sum;
	return true;
}
