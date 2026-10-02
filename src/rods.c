/*  rods.c — libPantheon: counting rod numerals, UTF-8.
   	 *
   	 *  True positional decimal on the counting board: one bundle of rods
   	 *  per column, the columns alternating between the two forms of the
   	 *  digits so a lone rod is never mistaken for its rotated twin.  The
   	 *  Unicode block encodes both (SMP, 4 bytes of UTF-8 each; every one
   	 *  shares the prefix F0 9D 8D, the last byte runs A0..B1):
   	 *
   	 *    U+1D360..U+1D368  COUNTING ROD UNIT DIGIT ONE..NINE   𝍠–𝍨
   	 *    U+1D369..U+1D371  COUNTING ROD TENS DIGIT ONE..NINE   𝍩–𝍱
   	 *
   	 *  The units column takes the unit set, the tens the tens set, the
   	 *  hundreds the unit set again — the Song orientation, and what the
   	 *  Unicode names assume.
   	 *
   	 *  The rods had no zero: an empty column was left literally empty
   	 *  and the reader counted columns.  We keep the blank as space, the
   	 *  cuneiform gap generalised to base ten — every place boundary
   	 *  writes exactly one space and a blank place writes one of its own,
   	 *  so a well-formed string holds exactly (places − 1) spaces:
   	 *
   	 *     𝍪 𝍢      = 23
   	 *     𝍠   𝍠     = 101    (the blank tens column)
   	 *     𝍠        = 100    (trailing blanks are spaces too)
   	 *
   	 *  That law is what makes the faithful blank round-trip: the parser
   	 *  sets places = spaces + 1, reads the leftmost glyph at place
   	 *  places − 1, and every glyph's form must match its place's
   	 *  parity — 𝍠 𝍠 for 11 is the rod DISH-before-a-U.
   	 *
   	 *  The signs are written as universal character names so this source
   	 *  stays pure ASCII.
   	 *
   	 *  Reference: https://en.wikipedia.org/wiki/Counting_rods
   	 */

#include <string.h>

#include "pantheon.h"

#define RODS_PREFIX   "\xF0\x9D\x8D" /* F0 9D 8D: the 3-byte stem of all 18 rods */
#define RODS_LO        0xA0          /* last byte of U+1D360 ROD UNIT ONE   */
#define RODS_MID       0xA9          /* last byte of U+1D369 ROD TENS ONE   */
#define RODS_HI        0xB1          /* last byte of U+1D371 ROD TENS NINE  */
#define SIGN_BYTES 4                 /* all rods are SMP: 4 bytes of UTF-8  */

/* set 0 = unit digits (even places), set 1 = tens digits (odd places). */
static const char *const signs[2][9] = {
	{ "\U0001D360", "\U0001D361", "\U0001D362", "\U0001D363", "\U0001D364",
	  "\U0001D365", "\U0001D366", "\U0001D367", "\U0001D368" },
	{ "\U0001D369", "\U0001D36A", "\U0001D36B", "\U0001D36C", "\U0001D36D",
	  "\U0001D36E", "\U0001D36F", "\U0001D370", "\U0001D371" },
};

static const uint64_t pow10[PN_ROD_MAX_PLACES] = {
	1ull, 10ull, 100ull, 1000ull, 10000ull, 100000ull, 1000000ull,
	10000000ull, 100000000ull, 1000000000ull, 10000000000ull,
	100000000000ull, 1000000000000ull, 10000000000000ull,
	100000000000000ull, 1000000000000000ull, 10000000000000000ull,
	100000000000000000ull, 1000000000000000000ull
};

size_t pn_rods_render_size(uint64_t n)
{
	/* Pure and total.  For n <= PN_ROD_MAX_VALUE this is exactly
	  * 4 * (count of nonzero digits) + (count of digits − 1): every place
	  * boundary from the top down to the units writes one space.  Larger
	  * n keeps counting as if the extra columns could still be drawn and
	  * the answer is simply meaningless — render rejects such n with
	  * pn_err_range. */
	size_t bytes = 0;
	int n_digits = 0;
	while (n != 0) {
		if (n % 10u != 0u)
			bytes += SIGN_BYTES;
		n_digits++;
		n /= 10u;
	}
	return bytes + (size_t)(n_digits > 0 ? n_digits - 1 : 0);
}

pn_status pn_rods_render(uint64_t n, char *buf, size_t buflen)
{
	if (buf == NULL)
		return pn_err_null;
	if (n > PN_ROD_MAX_VALUE)
		return pn_err_range;   /* a twentieth column: 10^19 no longer fits uint64_t */

	const size_t need = pn_rods_render_size(n); /* exact: buflen must fit it + NUL */
	if (buflen <= need)
		return pn_err_buffer;

	uint8_t dig[PN_ROD_MAX_PLACES];          /* dig[i] = digit at place i */
	int m = 0;
	while (n != 0) {
		dig[m++] = (uint8_t)(n % 10u);
		n /= 10u;
	}

	char *q = buf; /* safe: need is exact and validated against buflen */
	for (int i = m - 1; i >= 0; i--) {       /* most significant first */
		if (i != m - 1)
			*q++ = ' ';                       /* one space per place boundary */
		if (dig[i] != 0u) {
			const char *g = signs[i & 1][dig[i] - 1u];
			memcpy(q, g, SIGN_BYTES);
			q += SIGN_BYTES;
			}
		}
	*q = '\0';                               /* n == 0: just the blank board, and the NUL */
	return pn_ok;
}

/* --- parsing ------------------------------------------------------------
  	 *
  	 * The whole string is rods and spaces in canonical spacing.  The
  	 * blank column — no symbol for it for two thousand years — is a
  	 * space, so the total count of spaces is exactly (places − 1) and
  	 * the parser recovers places = spaces + 1: the leftmost glyph sits
  	 * at place places − 1, each run of r spaces advances the place by r,
  	 * and each glyph's set must match its place's parity (the
  	 * alternating orientation is the reader's column counter).  A
  	 * foreign byte, a sign cut off short, a leading space, two glyphs
  	 * touching, or a digit in the wrong column is malformed; more than
  	 * PN_ROD_MAX_PLACES places is beyond the board, not bad grammar —
  	 * the same ruling ten strokes get from the Egyptian scribe.
  	 */

/* Match a rod at p (which has ≥ SIGN_BYTES room to end): return the set
   * (0 unit, 1 tens) and the digit 1..9, or -1 for anything else. */
static int match_rod(const char *p, const char *end, int *digit)
{
	if (p + SIGN_BYTES > end || memcmp(p, RODS_PREFIX, 3) != 0)
		return -1;
	const unsigned char b = (unsigned char)p[3];
	if (b >= RODS_LO && b <= RODS_LO + 8u) { /* U+1D360..U+1D368 */
		*digit = (int)(b - RODS_LO) + 1;
		return 0;
	}
	if (b >= RODS_MID && b <= RODS_HI) {      /* U+1D369..U+1D371 */
		*digit = (int)(b - RODS_MID) + 1;
		return 1;
	}
	return -1;
}

pn_status pn_rods_parse(const char *buf, size_t buflen, uint64_t *out)
{
	if (buf == NULL || out == NULL)
		return pn_err_null;
	if (buflen == 0) {
			*out = 0;                   /* the empty board reads back as 0 */
		return pn_ok;
	}
	if (buf[0] == ' ')
		return pn_err_syntax;           /* the board never starts blank */

	size_t spaces = 0;                  /* count first: it defines the places */
	for (size_t i = 0; i < buflen; i++)
		if (buf[i] == ' ')
			spaces++;
	const int m = (int)(spaces + 1u);   /* top place is m − 1 */
	if (m > PN_ROD_MAX_PLACES)
		return pn_err_range;            /* more board than a uint64_t can hold */

	uint64_t sum = 0;                   /* build locally; *out untouched unless we succeed */
	int place = m - 1;
	int glyphs = 0;
	bool after_glyph = false;
	const char *p = buf, *end = buf + buflen;

	while (p < end) {
		if (*p == ' ') {
			place--;                    /* every space advances one place */
			after_glyph = false;
			p++;
			continue;
			}
		if (after_glyph)
			return pn_err_syntax;       /* two columns with no boundary space */

		int digit;
		const int set = match_rod(p, end, &digit);
		if (set < 0)
			return pn_err_syntax;       /* foreign byte, or a sign cut off short */
		if (set != (place & 1))
			return pn_err_syntax;       /* a rod in the wrong column */

			/* No overflow: place < m <= PN_ROD_MAX_PLACES caps the sum at
			  * 9 * (10^18 + ... + 1) = PN_ROD_MAX_VALUE. */
		sum += (uint64_t)digit * pow10[place];
		glyphs++;
		after_glyph = true;
		p += SIGN_BYTES;
		}

	if (glyphs == 0)
		return pn_err_syntax;           /* all spaces: not even a rod on the board */

	*out = sum;
	return pn_ok;
}
