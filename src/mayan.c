/*  mayan.c — libPantheon: Maya bar-and-dot numerals over pn_vigesimal.

    The renderer is thinner on purpose than babylonian.c over the
    same core.  Cuneiform builds each place out of tens-signs and
    units-signs and writes an empty place as two blank spaces
    (Unicode ships no Babylonian placeholder, and on the clay the
    gap really was ambiguous).  The Maya block, by contrast, hands
    over one ready-made sign for every place from 0 to 19 — the
    shell included.  So: one sign per place, one space per boundary.
    The string counts its own rows out loud; there is no spacing
    arithmetic around a two-space gap, and no lone dot needing
    context to rescue it.

    The signs are spelled as universal character names (C11
    escapes), which keeps this file pure ASCII; the Unicode chart
    checks every byte below:

    U+1D2E0..U+1D2F3  MAYAN NUMERAL ZERO..NINETEEN
                       UTF-8 stem F0 9D 8B, last byte A0..B3

    The codex stacks rows from the floor up (least significant at
    the bottom); our transcription reads the stack from the top
    down and lays it left to right — 429 renders as dot dot nine.

    References: https://en.wikipedia.org/wiki/Maya_numerals
    Unicode block Mayan Numerals (U+1D2E0..U+1D2F3), proposal
    L2/16-264 (Jameson Quinn), Unicode 10.0 (2017).
*/
#include <string.h>

#include "pantheon.h"

#define MAYAN_PREFIX     "\xF0\x9D\x8B" /* F0 9D 8B: the shared 3-byte stem of all 20 */
#define MAYAN_LO         0xA0           /* last byte of U+1D2E0, the shell             */
#define MAYAN_HI          0xB3          /* last byte of U+1D2F3, nineteen              */
#define SIGN_BYTES        4             /* all twenty are SMP: 4 UTF-8 bytes           */

/* digit_signs[d] = U+1D2E0 + d: bars, dots and shell, precomposed whole. */
static const char *const digit_signs[20] = {
	"\U0001D2E0", "\U0001D2E1", "\U0001D2E2", "\U0001D2E3", "\U0001D2E4",
	"\U0001D2E5", "\U0001D2E6", "\U0001D2E7", "\U0001D2E8", "\U0001D2E9",
	"\U0001D2EA", "\U0001D2EB", "\U0001D2EC", "\U0001D2ED", "\U0001D2EE",
	"\U0001D2EF", "\U0001D2F0", "\U0001D2F1", "\U0001D2F2", "\U0001D2F3",
};

/* A struct may only be carved from whole rows: every translation
   unit watches its own front door (sex60.c keeps a well_formed,
   babylonian.c keeps one, and so does this file). */
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

/* Match one digit sign at p (where at least SIGN_BYTES bytes remain):
   the digit 0..19 comes back in *digit, and anything else — the
   block's empty space past U+1D2F3, U+1D2F4..U+1D2FF, which is
   unassigned and carves nothing — gives -1. */
static int match_mayan(const char *p, const char *end, unsigned *digit)
{
	if (p + SIGN_BYTES > end || memcmp(p, MAYAN_PREFIX, 3) != 0)
		return -1;
	const unsigned char b = (unsigned char)p[3];

	if (b >= MAYAN_LO && b <= MAYAN_HI) {    /* U+1D2E0..U+1D2F3 */
		*digit = (unsigned)(b - MAYAN_LO);
		return 0;
	}
	return -1;
}

size_t pn_mayan_render_size(pn_vigesimal v)
{
	/* This answer never fails: '-' when neg, one sign per place, one
	 * gutter per boundary.  For a well-formed v it is exactly what
	 * render will write; on a malformed one the count simply runs on
	 * and means nothing — render turns down such v with
	 * PN_ERROR_DOMAIN anyway. */
	size_t n = v.neg ? 1u : 0u;

	for (unsigned i = 0; i < v.n_places; i++) {
		if (i > 0)
			n += 1u;                         /* one gutter between rows */
		n += SIGN_BYTES;
	}
	return n;
}

bool pn_mayan_render(pn_vigesimal v, char *_Nonnull buf, size_t buflen,
                     pn_error *_Nullable *_Nullable error)
{
	if (buf == NULL)
		return pn_error_set(error, PN_ERROR_NULL,
				                    "render needs a buffer to write into");
	if (!well_formed(v))
		return pn_error_set(error, PN_ERROR_DOMAIN,
				                    "the value is not well-formed:"
				                    " %u places, radix %u — places need"
				                    " 1 to %u slots, each 0 to 19, and"
				                    " the radix cannot pass n_places",
				                   v.n_places, v.radix,
				                   PN_VIGESIMAL_MAX_PLACES);

	/* render_size is exact: buflen has to hold its bytes plus NUL */
	const size_t need = pn_mayan_render_size(v);

	if (buflen <= need)
		return pn_error_set(error, PN_ERROR_BUFFER,
				                    "buffer holds %zu bytes; this value"
				                    " needs %zu plus the terminating NUL",
				                   buflen, need);

	char *q = buf;    /* safe: need is exact and already checked */

	if (v.neg)
		*q++ = '-';                          /* an anachronism, carved anyway */
	for (unsigned i = 0; i < v.n_places; i++) {
		if (i > 0)
			*q++ = ' ';
		memcpy(q, digit_signs[v.places[i]], SIGN_BYTES);
		q += SIGN_BYTES;
	}
	*q = '\0';
	return true;
}

/* Parsing reads back what rendering wrote.

   The grammar is '-? (sign ' ')* sign' — rows that count themselves.
   The shell is welcome anywhere a whole place sits: a zero row whose
   blank a reader would otherwise have to guess at is exactly what
   the shell exists to spell, and a shell at the front is kept
   exactly as written, since the reader counts rows off the gutters.
   More rows than the codex page holds
   (PN_VIGESIMAL_MAX_PLACES) is the scribe's page overrun:
   PN_ERROR_RANGE, not a grammar fault. */
bool pn_mayan_parse(const char *_Nullable buf, size_t buflen,
                    pn_vigesimal *_Nullable out,
                    pn_error *_Nullable *_Nullable error)
{
	if (buf == NULL || out == NULL)
		return pn_error_set(error, PN_ERROR_NULL,
				                    "parse needs a string and a place to"
				                    " put the value");
	if (buflen == 0)
		return pn_error_set(error, PN_ERROR_SYNTAX,
				                    "an empty tablet is no number:"
				                    " the shell is how zero spells"
				                    " itself");

	const char *p = buf, *end = buf + buflen;

	pn_vigesimal v;  /* *out keeps its old value unless all works */

	memset(&v, 0, sizeof v);

	if (*p == '-') {
		if (p + 1 >= end)
			return pn_error_set(error, PN_ERROR_SYNTAX,
				                          "a '-' sitting over nothing");
		v.neg = true;
		p++;
	}

	unsigned k = 0;

	for (;;) {
		unsigned digit;

		if (match_mayan(p, end, &digit) != 0)
			return pn_error_set(error, PN_ERROR_SYNTAX,
				                           "%zu byte(s) at position %zu"
				                           " are neither a whole digit"
				                           " sign nor a single joining"
				                           " space (doubled gutter or"
				                           " two signs touching look"
				                           " the same from here)",
				                           (size_t)(end - p),
				                           (size_t)(p - buf));
		if (k == PN_VIGESIMAL_MAX_PLACES)
			return pn_error_set(error, PN_ERROR_RANGE,
				                           "the row past the page:"
				                           " a codex stack holds %u",
				                           PN_VIGESIMAL_MAX_PLACES);
		v.places[k++] = (uint8_t)digit;
		p += SIGN_BYTES;
		if (p == end)
			break;
		if (*p != ' ' || p + 1 >= end)
			return pn_error_set(error, PN_ERROR_SYNTAX,
				                           "rows join with one space:"
				                           " position %zu wears a"
				                           "%s",
				                           (size_t)(p - buf),
				                           (*p == ' ') ? " trailing"
				                            " space with no row under"
				                            " it" : " no gutter");
		p++;
	}
	v.n_places = (uint8_t)k;
	v.radix = v.n_places;   /* the carved stack carries no point:
	                          * period style, the same answer the
	                          * cuneiform parser gives                */
	*out = v;
	return true;
}
