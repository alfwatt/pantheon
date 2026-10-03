/*  mayan.c — libPantheon: Maya bar-and-dot numerals over pn_vigesimal.
 *
 *  The renderer is deliberately thinner than babylonian.c over the same
 *  core: where cuneiform composes each place from tens and units signs
 *  and writes a zero place as two empty spaces (Unicode encodes no
 *  Babylonian placeholder, and on the clay the gap was genuinely
 *  ambiguous), the Maya block hands us one precomposed sign for every
 *  place 0..19 — shell included.  One sign per place, one space per
 *  boundary: the string counts its own rows, no spacing arithmetic à
 *  la the two-space gap, no context rescuing a lone dot.
 *
 *  The signs are universal character names (C11 escapes) so this file
 *  stays pure ASCII; the Unicode chart verifies every byte below:
 *
 *    U+1D2E0..U+1D2F3  MAYAN NUMERAL ZERO..NINETEEN
 *                       UTF-8 stem F0 9D 8B, tail byte A0..B3
 *
 *  The codex stacks rows bottom-up (least significant at the floor);
 *  the transcription reads the stack from its top downward, laid left
 *  to right — 429 renders dot dot nine.
 *
 *  References: https://en.wikipedia.org/wiki/Maya_numerals
 *  Unicode block Mayan Numerals (U+1D2E0..U+1D2F3), proposal
 *  L2/16-264 (Jameson Quinn), Unicode 10.0 (2017).
 */

#include <string.h>

#include "pantheon.h"

#define MAYAN_PREFIX   "\xF0\x9D\x8B" /* F0 9D 8B: the 3-byte stem of all 20 */
#define MAYAN_LO        0xA0           /* last byte of U+1D2E0 the shell      */
#define MAYAN_HI        0xB3           /* last byte of U+1D2F3 nineteen       */
#define SIGN_BYTES      4              /* all twenty are SMP: 4 bytes UTF-8   */

/* digit_signs[d] = U+1D2E0 + d: bar, dots and shell precomposed whole. */
static const char *const digit_signs[20] = {
	"\U0001D2E0", "\U0001D2E1", "\U0001D2E2", "\U0001D2E3", "\U0001D2E4",
	"\U0001D2E5", "\U0001D2E6", "\U0001D2E7", "\U0001D2E8", "\U0001D2E9",
	"\U0001D2EA", "\U0001D2EB", "\U0001D2EC", "\U0001D2ED", "\U0001D2EE",
	"\U0001D2EF", "\U0001D2F0", "\U0001D2F1", "\U0001D2F2", "\U0001D2F3",
};

/* A struct may only be carved from whole rows: each translation unit
 * guards its own front door (sex60.c keeps its well_formed, so does
 * babylonian.c; so does this). */
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

size_t pn_mayan_render_size(pn_vigesimal v)
{
	/* Pure and total: '-' when neg, one sign per place, one gutter per
	 * boundary — for a well-formed v exactly what render writes; on a
	 * malformed one the count runs on meaninglessly and render rejects
	 * such v with pn_err_domain. */
	size_t n = v.neg ? 1u : 0u;
	for (unsigned i = 0; i < v.n_places; i++) {
		if (i > 0)
			n += 1u;                       /* one gutter between rows */
		n += SIGN_BYTES;
	}
	return n;
}

pn_status pn_mayan_render(pn_vigesimal v, char *buf, size_t buflen)
{
	if (buf == NULL)
		return pn_err_null;
	if (!well_formed(v))
		return pn_err_domain;

	const size_t need = pn_mayan_render_size(v); /* exact: buflen must fit + NUL */
	if (buflen <= need)
		return pn_err_buffer;

	char *q = buf;                          /* safe: need exact, validated */
	if (v.neg)
		*q++ = '-';                         /* an anachronism, carved anyway */
	for (unsigned i = 0; i < v.n_places; i++) {
		if (i > 0)
			*q++ = ' ';
		memcpy(q, digit_signs[v.places[i]], SIGN_BYTES);
		q += SIGN_BYTES;
	}
	*q = '\0';
	return pn_ok;
}

/* --- parsing ------------------------------------------------------------
 *
 * Grammar: '-? (sign ' ')* sign' — self-counting rows.  The shell is
 * legal anywhere a whole place sits (a zero row whose blank would have
 * to be inferred is exactly what the shell is FOR), and a leading shell
 * is kept literally, the reader counting rows off the gutters.  More
 * rows than the codex page holds (PN_VIGESIMAL_MAX_PLACES) is the
 * scribe's page overrun: pn_err_range, not syntax.
 */

/* Match a digit sign at p (which has >= SIGN_BYTES room): the digit
 * 0..19, or -1 for anything else — including the block's void,
 * U+1D2F4..U+1D2FF, which is unassigned and carves nothing. */
static int match_mayan(const char *p, const char *end, unsigned *digit)
{
	if (p + SIGN_BYTES > end || memcmp(p, MAYAN_PREFIX, 3) != 0)
		return -1;
	const unsigned char b = (unsigned char)p[3];
	if (b >= MAYAN_LO && b <= MAYAN_HI) {   /* U+1D2E0..U+1D2F3 */
		*digit = (unsigned)(b - MAYAN_LO);
		return 0;
	}
	return -1;
}

pn_status pn_mayan_parse(const char *buf, size_t buflen, pn_vigesimal *out)
{
	if (buf == NULL || out == NULL)
		return pn_err_null;
	if (buflen == 0)
		return pn_err_syntax;               /* an empty tablet is no number:
		                                     * the shell spells zero */

	const char *p = buf, *end = buf + buflen;
	pn_vigesimal v;                         /* *out untouched unless we win */
	memset(&v, 0, sizeof v);

	if (*p == '-') {
		if (p + 1 >= end)
			return pn_err_syntax;           /* a '-' over nothing */
		v.neg = true;
		p++;
	}

	unsigned k = 0;
	for (;;) {
		unsigned digit;
		if (match_mayan(p, end, &digit) != 0)
			return pn_err_syntax;           /* foreign byte, short UTF-8,
		                                     * doubled gutter, touching signs */
		if (k == PN_VIGESIMAL_MAX_PLACES)
			return pn_err_range;            /* the row past the page */
		v.places[k++] = (uint8_t)digit;
		p += SIGN_BYTES;
		if (p == end)
			break;
		if (*p != ' ' || p + 1 >= end)
			return pn_err_syntax;           /* jammed or trailing gutter */
		p++;
	}
	v.n_places = (uint8_t)k;
	v.radix = v.n_places;                   /* the carved stack carries no
	                                         * point: period style, as the
	                                         * cuneiform parser answers */
	*out = v;
	return pn_ok;
}
