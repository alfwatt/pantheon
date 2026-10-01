/*  babylonian.c — libPantheon: positional cuneiform numerals, UTF-8.
 *
 *  Two signs do all the work, exactly as on the clay:
 *
 *    U+12079 CUNEIFORM SIGN DISH  (vertical wedge) = 1
 *    U+1230B CUNEIFORM SIGN U     (corner wedge)    = 10
 *
 *  A place of value d is (d/10) copies of U followed by (d%10) copies of
 *  DISH; places are separated by one space.  Unicode encodes no zero
 *  placeholder for the sexagesimal gap, so an empty place is rendered as
 *  two spaces — the same blank a scribe left, with the same ambiguity:
 *  on the tablet, a lone DISH meant 1 or 60 or 1/60 by context alone.
 *
 *  The signs are written as universal character names so this source
 *  stays pure ASCII.  Note we deliberately do NOT use the precomposed
 *  signs of Unicode's "Cuneiform Numbers and Punctuation" block
 *  (U+12400..): those are metrology signs, not school-text positional
 *  digits.
 *
 *  Reference: https://en.wikipedia.org/wiki/Babylonian_numerals
 */

#include <string.h>

#include "pantheon.h"

#define DISH_UTF8 "\U00012079"   /* UTF-8: F0 92 81 B9 */
#define U_UTF8     "\U0001230B"  /* UTF-8: F0 92 8C 8B */
#define SIGN_BYTES 4             /* both signs are 4 bytes of UTF-8 */
#define GAP_BYTES  2             /* an empty place: two spaces */

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

/* Bytes a group of wedges needs for one place: d == 0 is the gap. */
static size_t group_bytes(unsigned d)
{
	return (d == 0) ? GAP_BYTES : (d / 10u + d % 10u) * SIGN_BYTES;
}

size_t pn_babylonian_render_size(pn_sex60 v)
{
	size_t n = v.neg ? 1u : 0u;
	for (unsigned i = 0; i < v.n_places; i++) {
		if (i > 0)
			n += 1u;                 /* one space between places */
		n += group_bytes(v.places[i]);
	}
	return n;
}

pn_status pn_babylonian_render(pn_sex60 v, char *buf, size_t buflen)
{
	if (buf == NULL)
		return pn_err_null;
	if (!well_formed(v))
		return pn_err_domain;

	size_t need = pn_babylonian_render_size(v); /* exact: buflen must fit it + NUL */
	if (buflen <= need)
		return pn_err_buffer;

	char *q = buf; /* safe: need is exact and validated against buflen */
	if (v.neg)
		*q++ = '-';                    /* an anachronism; tablets had no sign */
	for (unsigned i = 0; i < v.n_places; i++) {
		if (i > 0)
			*q++ = ' ';
		unsigned d = v.places[i];
		if (d == 0) {
			*q++ = ' ';
			*q++ = ' ';            /* the historical gap */
			continue;
		}
		for (unsigned j = 0; j < d / 10u; j++) {
			memcpy(q, U_UTF8, SIGN_BYTES);
			q += SIGN_BYTES;
		}
		for (unsigned j = 0; j < d % 10u; j++) {
			memcpy(q, DISH_UTF8, SIGN_BYTES);
			q += SIGN_BYTES;
		}
	}
	*q = '\0';
	return pn_ok;
}

/* --- parsing ----------------------------------------------------------------
 *
 * A rendered string is a sequence of wedge groups separated by space
 * runs, so parsing is tokenizing into the two: runs of spaces, and
 * runs of signs.  The renderer's spacing law decides what is legal:
 *
 *   leading run   3k    (k zero places before the first group)
 *   between runs  3k+1  (one joiner plus k two-space gaps)
 *   trailing run  3k    (k zero places after the last group)
 *   all-space     3k-1  (k zero places, no group at all)
 *
 * Everything else — other bytes, other spacings, groups written out
 * of canonical order — is rejected, which is what makes the
 * parse-after-render round trip exact.
 * -------------------------------------------------------------------------- */

pn_status pn_babylonian_parse(const char *buf, size_t buflen, pn_sex60 *out)
{
	if (buf == NULL || out == NULL)
		return pn_err_null;

	pn_sex60 v; /* build locally; *out is untouched unless we succeed */
	memset(&v, 0, sizeof v);

	const char *p = buf, *end = buf + buflen;
	if (buflen > 0 && *p == '-') {
		v.neg = true;
		p++;
	}

	size_t spaces = 0;   /* pending space run */
	int got_group = 0;
	size_t n = 0;        /* places stored */

	while (p < end) {
		if (*p == ' ') {
			const char *s0 = p;
			while (p < end && *p == ' ')
				p++;
			spaces += (size_t)(p - s0);
			continue;
		 }

		 /* A group begins here: flush the pending spaces first. */
		size_t zeros;
		if (!got_group) {             /* leading run: a multiple of 3 */
			if (spaces % 3u != 0u)
				return pn_err_syntax;
			zeros = spaces / 3u;
		} else {                      /* between run: 3k + 1 */
			if ((spaces - 1u) % 3u != 0u)
				return pn_err_syntax;
			zeros = (spaces - 1u) / 3u;
		}
		if (n + zeros > PN_SEX60_MAX_PLACES)
			return pn_err_range;
		for (; zeros > 0; zeros--)
			v.places[n++] = 0;

		/* The group itself: U signs then DISH signs, canonical order
		 	* and canonical counts only (a fifth ten plus nine units is
			* still under 60; a sixth ten is not a place). */
		size_t nu = 0, nd = 0;
		while (p + SIGN_BYTES <= end && memcmp(p, U_UTF8, SIGN_BYTES) == 0) {
			p += SIGN_BYTES;
			if (++nu > 5u)
				return pn_err_range;
		}
		while (p + SIGN_BYTES <= end && memcmp(p, DISH_UTF8, SIGN_BYTES) == 0) {
			p += SIGN_BYTES;
			if (++nd > 9u)
				return pn_err_range;
		}
		if (nu + nd == 0u)
			return pn_err_syntax;     /* any other byte, or a cut-off sign */
		if (p != end && *p != ' ')
			return pn_err_syntax;     /* e.g. a U glued after a DISH */
		v.places[n++] = (uint8_t)(nu * 10u + nd);
		got_group = 1;
		spaces = 0;
	}

	if (!got_group) {                 /* all-space: 3k - 1 spaces, k zero places */
		if ((spaces + 1u) % 3u != 0u)
			return pn_err_syntax;     /* "" and "-" land here too */
		size_t k = (spaces + 1u) / 3u;
		if (k > PN_SEX60_MAX_PLACES)
			return pn_err_range;
		for (; k > 0; k--)
			v.places[n++] = 0;
	} else if (spaces % 3u != 0u) {   /* trailing run: a multiple of 3 */
		return pn_err_syntax;
	} else {
		size_t zeros = spaces / 3u;
		if (n + zeros > PN_SEX60_MAX_PLACES)
			return pn_err_range;
		for (; zeros > 0; zeros--)
			v.places[n++] = 0;
	}

	if (n == 0)
		return pn_err_syntax;         /* unreachable; belt and tablet-clay */

	v.n_places = (uint8_t)n;
	v.radix = (uint8_t)n;             /* no radix point on the tablet: integers */
	*out = v;
	return pn_ok;
}
