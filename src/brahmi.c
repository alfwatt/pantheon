/*  brahmi.c — libPantheon: Brahmi numerals, UTF-8.
	*
	*  Twenty signs, descending four slots — thousands, hundreds, tens,
	*  ones — and unlike Egypt no digit is a repetition: every unit 1..9
	*  and every ten 10..90 has its own dedicated sign, so 47 is two
	*  signs, not eleven:
	*
	*    U+11052..U+1105A BRAHMI NUMBER ONE..NINE      = 1..9
	*    U+1105B..U+11063 BRAHMI NUMBER TEN..NINETY    = 10..90
	*    U+11064          BRAHMI NUMBER ONE HUNDRED    = 100
	*    U+11065          BRAHMI NUMBER ONE THOUSAND   = 1 000
	*    U+1107F          BRAHMI NUMBER JOINER         (ligature join)
	*
	*  Hundreds and thousands multiply: a digit 2..9 ligatured with the
	*  power sign — power first, then the joiner, then the multiplier —
	*  so 400 is 𑁤𑁿𑁕 rendered as one stacked ligature, while an additive
	*  component stays unligated: 104 is 𑁤𑁕 (Unicode 14.x's ruling; the
	*  joiner marks the semantic difference and is never dropped).  A
	*  multiplier of 1 needs no ligature: the power sign alone is 100.
	*
	*  There is no sign above the thousand and no zero — a lone blank is
	*  the nothing owed — so 0 renders as the empty string and anything
	*  past 9 999 is out of range rather than absurd.
	*
	*  The parser reads the four slots, not the signs: 2 222 writes 𑁓
	*  three times — thousands multiplier, hundreds multiplier, ones
	*  term — and only a second term in one slot is a blunder.  Two bare
	*  power signs in a row, a joiner without its multiplier, a ligature
	*  left unligated (𑁤𑁕𑁕: 100+4+4, and no slot allows the second 𑁕) or
	*  ascending on the pillar are all rejected the way a reader rejects
	*  "IIIX".
	*
	*  The signs are written as universal character names so this source
	*  stays pure ASCII.
	*
	*  Reference: https://en.wikipedia.org/wiki/Brahmi_numerals
	*/

#include <string.h>

#include "pantheon.h"

#define ONE      "\U00011052" /* 𑁒 = 1      UTF-8: F0 91 81 92 */
#define TWO      "\U00011053" /* 𑁓 = 2      UTF-8: F0 91 81 93 */
#define THREE    "\U00011054" /* 𑁔 = 3      UTF-8: F0 91 81 94 */
#define FOUR     "\U00011055" /* 𑁕 = 4      UTF-8: F0 91 81 95 */
#define FIVE     "\U00011056" /* 𑁖 = 5      UTF-8: F0 91 81 96 */
#define SIX      "\U00011057" /* 𑁗 = 6      UTF-8: F0 91 81 97 */
#define SEVEN    "\U00011058" /* 𑁘 = 7      UTF-8: F0 91 81 98 */
#define EIGHT    "\U00011059" /* 𑁙 = 8      UTF-8: F0 91 81 99 */
#define NINE     "\U0001105A" /* 𑁚 = 9      UTF-8: F0 91 81 9A */
#define TEN      "\U0001105B" /* 𑁛 = 10     UTF-8: F0 91 81 9B */
#define TWENTY   "\U0001105C" /* 𑁜 = 20     UTF-8: F0 91 81 9C */
#define THIRTY   "\U0001105D" /* 𑁝 = 30     UTF-8: F0 91 81 9D */
#define FORTY    "\U0001105E" /* 𑁞 = 40     UTF-8: F0 91 81 9E */
#define FIFTY    "\U0001105F" /* 𑁟 = 50     UTF-8: F0 91 81 9F */
#define SIXTY    "\U00011060" /* 𑁠 = 60     UTF-8: F0 91 81 A0 */
#define SEVENTY  "\U00011061" /* 𑁡 = 70     UTF-8: F0 91 81 A1 */
#define EIGHTY   "\U00011062" /* 𑁢 = 80     UTF-8: F0 91 81 A2 */
#define NINETY   "\U00011063" /* 𑁣 = 90     UTF-8: F0 91 81 A3 */
#define HUNDRED  "\U00011064" /* 𑁤 = 100    UTF-8: F0 91 81 A4 */
#define THOUSAND "\U00011065" /* 𑁥 = 1 000  UTF-8: F0 91 81 A5 */
#define JOINER   "\U0001107F" /* 𑁿 ligature join, numbers only */
#define SIGN_BYTES 4                /* every sign is astral: 4 bytes of UTF-8 */

static const char *const units[9] = {
	ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE
};

static const char *const tens[9] = {
	TEN, TWENTY, THIRTY, FORTY, FIFTY, SIXTY, SEVENTY, EIGHTY, NINETY
};

/* Emit n's canonical Brahmi string; buf may be NULL, which merely counts.
	* Pure and total: n > PN_BRAHMI_MAX_VALUE simply drops the places above
	* the thousand and the answer is meaningless — render rejects such n
	* with pn_err_range. */
static size_t brahmi_emit(uint64_t n, char *buf)
{
	size_t bytes = 0;

#define PUT(s) do {                                                          \
		if (buf != NULL)                                                     \
			memcpy(buf + bytes, (s), SIGN_BYTES);                            \
		bytes += SIGN_BYTES;                                                 \
	} while (0)

	/* A power term: the power sign bare for digit 1, ligatured
	  	* (power, joiner, multiplier) for digits 2..9, silent for 0. */
#define PUT_POWER(power, d) do {                                             \
		if ((d) >= 1u)                                                       \
			PUT(power);                                                      \
		if ((d) >= 2u) {                                                     \
			PUT(JOINER);                                                     \
			PUT(units[(d) - 1u]);                                            \
		}                                                                    \
	} while (0)

	const unsigned d4 = (unsigned)((n / 1000u) % 10u);
	const unsigned d3 = (unsigned)((n / 100u) % 10u);
	const unsigned d2 = (unsigned)((n / 10u) % 10u);
	const unsigned d1 = (unsigned)(n % 10u);

	PUT_POWER(THOUSAND, d4);
	PUT_POWER(HUNDRED, d3);
	if (d2 != 0u)
		PUT(tens[d2 - 1u]);
	if (d1 != 0u)
		PUT(units[d1 - 1u]);

#undef PUT_POWER
#undef PUT
	return bytes;
}

size_t pn_brahmi_render_size(uint64_t n)
{
	return brahmi_emit(n, NULL);
}

pn_status pn_brahmi_render(uint64_t n, char *buf, size_t buflen)
{
	if (buf == NULL)
		return pn_err_null;
	if (n > PN_BRAHMI_MAX_VALUE)
		return pn_err_range;  /* no sign above the thousand: ten thousand is words, not signs */

	const size_t need = brahmi_emit(n, NULL); /* exact: buflen must fit it + NUL */
	if (buflen <= need)
		return pn_err_buffer;

	(void)brahmi_emit(n, buf);
	buf[need] = '\0';         /* n == 0: just the blank, and the NUL */
	return pn_ok;
}

/* --- parsing ------------------------------------------------------------
	*
	* Canonical form is what makes the round trip exact: the twenty number
	* signs and the joiner, read as at most four descending terms —
	* thousands, hundreds, tens, ones — each term at most once, with no
	* separators.  A hundreds or thousands term is either the bare power
	* sign (digit 1) or the ligature <power JOINER units-digit> (digits
	* 2..9); the ones and tens terms are their single dedicated signs.
	* Anything else — foreign bytes, a cut-off sign, a term out of its
	* slot, a joiner joining anything but a multiplier to its power, a
	* 1 × 100 ligature that should have been the bare sign, or an
	* unligated 400 where the scribe would have joined it — is rejected
	* the way a reader rejects "IIIX".
	*/

/* The whole sign inventory, in code-point order: units, tens, powers,
	* joiner — the joiner is never a term and never leads a match. */
#define N_SIGNS 21
static const char *const signs[N_SIGNS] = {
	ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE,
	TEN, TWENTY, THIRTY, FORTY, FIFTY, SIXTY, SEVENTY, EIGHTY, NINETY,
	HUNDRED, THOUSAND, JOINER
};
#define SIG_NINE    8
#define SIG_TEN     9
#define SIG_NINETY  17
#define SIG_HUNDRED   18
#define SIG_THOUSAND  19
#define SIG_JOINER    20

/* Match a whole sign at p: its index, or -1 for a foreign byte or a
	* sign cut off short. */
static int match_sign(const char *p, const char *end)
{
	for (int i = 0; i < N_SIGNS; i++)
		if (p + SIGN_BYTES <= end && memcmp(p, signs[i], SIGN_BYTES) == 0)
			return i;
	return -1;
}

pn_status pn_brahmi_parse(const char *buf, size_t buflen, uint64_t *out)
{
	if (buf == NULL || out == NULL)
		return pn_err_null;
	if (buflen == 0) {
		*out = 0;                    /* the blank reads back as the nothing it was */
		return pn_ok;
	}

	uint64_t sum = 0;       /* build locally; *out untouched unless we succeed */
	int slot = 0;           /* lowest term not yet spoken: 0 thousands .. 3 ones */
	const char *p = buf, *end = buf + buflen;

	while (p < end) {
		const int s = match_sign(p, end);
		if (s < 0)
			return pn_err_syntax;   /* foreign byte, or a sign cut off short */
		if (s == SIG_JOINER)
			return pn_err_syntax;   /* a joiner nothing is being joined to:
			                           * doubled, leading, or off its power sign */
		p += SIGN_BYTES;

		uint64_t worth;
		int next;                   /* the slot the next term must clear */

		if (s == SIG_THOUSAND || s == SIG_HUNDRED) {
			const int is_thousand = (s == SIG_THOUSAND);
			const uint64_t power = is_thousand ? 1000u : 100u;
			if (slot > (is_thousand ? 0 : 1))
				return pn_err_syntax;  /* a second term of this power, or an
				                        * ascending one: the thousand leads */

			/* The digit: the bare power is 1; the ligature <JOINER
			  	* units-digit> multiplies it. */
			worth = power;
			if (p + SIGN_BYTES <= end &&
					memcmp(p, JOINER, SIGN_BYTES) == 0) {
				p += SIGN_BYTES;
				const int c = match_sign(p, end);
				if (c < 0 || c > SIG_NINE)
					return pn_err_syntax;   /* joined to a non-multiplier —
				                             * a tens sign, a power, a second
				                             * joiner — or to nothing */
				if (c == 0)
					return pn_err_syntax;   /* 1 × 1000: write the sign bare */
				worth = (uint64_t)(c + 1) * power;
				p += SIGN_BYTES;
			}
			next = is_thousand ? 1 : 2;
		} else if (s >= SIG_TEN && s <= SIG_NINETY) {
			if (slot > 2)
				return pn_err_syntax;   /* a tens term twice, or after the ones */
			worth = (uint64_t)(s - SIG_TEN + 1) * 10u;
			next = 3;
		} else {                        /* a bare units sign: the ones term */
			if (slot > 3)
				return pn_err_syntax;   /* the ones term is already spoken */
			worth = (uint64_t)(s + 1);
			next = 4;
		}

		/* No overflow: four slots at their fullest are
		  	* 9 000 + 900 + 90 + 9 = PN_BRAHMI_MAX_VALUE. */
		sum += worth;
		slot = next;
	}

	*out = sum;
	return pn_ok;
}
