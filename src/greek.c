/* greek.c — Greek alphabetic (Milesian/Ionic) numerals, UTF-8.
 *
 * The uint64-direct rule book: no place-value struct layer between the
 * number and its cord, same as the egyptian scribe, the counting rods
 * and the quipu — an additive notation has no places to hold.
 *
 * The cord: an optional low keraia (U+0375) announcing the thousands
 * run, the thousands run, ONE separating space when both runs are
 * alive, the units run, always the trailing keraia (U+0374).  A run
 * is the three decimal digits read as letters — at most one sign of
 * the hundreds row (rho..sampi), then one of the tens row (iota..
 * koppa), then one of the units row (alpha..theta); the digit d of a
 * row is its row's d-th sign, ONCE: the market kept a dedicated
 * letter for every one of the 27 digits, so 900 is sampi, not nine
 * sampis.  The space kills the 972 butt-joint collisions (10001
 * ͵ι|α against 11000 ͵ια|) and belongs to the canonical cord.
 *
 * Every sign here is a 2-byte BMP code point: the encoder writes
 * canonical 2-byte sequences only, and the decoder admits no code
 * point outside the 27 numeral signs and the two keraia — capitals,
 * the lunate sigma U+03F2, the final sigma, the digamma U+03DD, the
 * tonos and the NFC twin of the dexia (U+02B9) hold no table entry
 * and fall out as syntax errors, which is exactly the culture's own
 * verdict on a misspelled number.
 */

#include <string.h>

#include "pantheon.h"

#define GREEK_LOWK   0x375u /* ͵ the low keraia: announces the thousands  */
#define GREEK_DEXIA  0x374u /* ʹ the trailing keraia: proof of a number   */
#define GREEK_SPACE  0x020u /* the run separator, a canonical byte        */
#define GREEK_MAX_TOKS 12u  /* low keraia, 3 signs, space, 3 signs, dexia:
                                nine tokens; a tenth is no cord            */

/*  The three rows, descending: row j (0 = hundreds, 1 = tens,
 *  2 = units) holds its nine signs; sign i (0..8) is worth (i+1)
 *  times the row's scale.  The descent rank is 3 - j. */
static const uint32_t row_hund[9] = {
	0x3C1u, 0x3C3u, 0x3C4u, 0x3C5u, 0x3C6u, 0x3C7u, 0x3C8u, 0x3C9u, 0x3E1u
};
static const uint32_t row_ten[9] = {
	0x3B9u, 0x3BAu, 0x3BBu, 0x3BCu, 0x3BDu, 0x3BEu, 0x3BFu, 0x3C0u, 0x3DFu
};
static const uint32_t row_unit[9] = {
	0x3B1u, 0x3B2u, 0x3B3u, 0x3B4u, 0x3B5u, 0x3DBu, 0x3B6u, 0x3B7u, 0x3B8u
};
static const uint32_t *const rows[3] = { row_hund, row_ten, row_unit };

/*  Count the live digits of k (k <= 999): each nonzero decimal digit
 *  is its row's dedicated sign, once; the blank digit writes nothing. */
static unsigned run_signs(uint64_t k)
{
	unsigned c = 0u;

	while (k != 0u) {
		if ((k % 10u) != 0u)
			c++;
		k /= 10u;
	}
	return c;
}

/*  Canonical 2-byte UTF-8 of one BMP code point below 0x800 — every
 *  sign in the tables above obeys; the two keraia are CD B4/B5. */
static void put_sign(char *buf, size_t *w, uint32_t cp)
{
	buf[(*w)++] = (char)(0xC0u | ((cp >> 6) & 0x1Fu));
	buf[(*w)++] = (char)(0x80u | (cp & 0x3Fu));
}

/*  Write the run of k (k <= 999): the rows descending, one sign per
 *  live digit. */
static void put_run(uint64_t k, char *buf, size_t *w)
{
	unsigned j;

	for (j = 0u; j < 3u; j++) {
		uint64_t scale = (uint64_t[3]){ 100u, 10u, 1u }[j];
		unsigned d = (unsigned)((k / scale) % 10u);
		if (d != 0u)
			put_sign(buf, w, rows[j][d - 1u]);
	}
}

size_t pn_greek_render_size(uint64_t n)
{
	uint64_t th, un;
	size_t bytes;

	if (n == 0u || n > PN_GREEK_MAX_VALUE)
		return 0u; /* the blank cord; the range check is render's job */
	th = n / 1000u;
	un = n % 1000u;
	bytes = 2u * (size_t)(run_signs(th) + run_signs(un));
	if (th != 0u)
		bytes += 2u;                  /* the low keraia           */
	bytes += 2u;                      /* the trailing keraia      */
	if (th != 0u && un != 0u)
		bytes += 1u;                  /* the run-separating space */
	return bytes;
}

pn_status pn_greek_render(uint64_t n, char *buf, size_t buflen)
{
	uint64_t th, un;
	size_t w;

	if (buf == NULL)
		return pn_err_null;
	if (n > PN_GREEK_MAX_VALUE)
		return pn_err_range;
	if (buflen < pn_greek_render_size(n) + 1u)
		return pn_err_buffer;

	th = n / 1000u;
	un = n % 1000u;
	w = 0u;
	if (n != 0u) {
		if (th != 0u) {
			put_sign(buf, &w, GREEK_LOWK);
			put_run(th, buf, &w);
			if (un != 0u)
				buf[w++] = ' ';
		}
		put_run(un, buf, &w);
		put_sign(buf, &w, GREEK_DEXIA);
	}
	buf[w] = '\0';
	return pn_ok;
}

/*  Look a decoded code point up in the three rows.  *rank (3, 2, 1:
 *  the descent a run must follow) and *val (its worth) come back
 *  only for a sign with a table entry; everything else — capitals,
 *  the lunate and final sigmas, the digamma, the tonos, the prime
 *  twin, Latin — misses and becomes syntax. */
static int sign_info(uint32_t cp, unsigned *rank, unsigned *val)
{
	unsigned j, i;

	for (j = 0u; j < 3u; j++) {
		uint64_t scale = (uint64_t[3]){ 100u, 10u, 1u }[j];
		for (i = 0u; i < 9u; i++) {
			if (cp == rows[j][i]) {
				*rank = 3u - j;
					*val = (unsigned)((i + 1u) * scale);
				return 1;
				}
			}
		}
	return 0;
}

/*  The value of a legal run — at most one sign per row, the rows
 *  strictly descending, every sign in the tables — or 0 when the
 *  run is empty or malformed.  No legal number's run is worth 0:
 *  a blank writes nothing and a written row is worth its sign. */
static uint64_t run_value(const uint32_t *toks, size_t len)
{
	uint64_t sum = 0u;
	unsigned prev_rank = 4u;
	size_t i;

	for (i = 0u; i < len; i++) {
		unsigned rank, val;
		if (toks[i] == GREEK_LOWK || toks[i] == GREEK_DEXIA ||
		    toks[i] == GREEK_SPACE ||
		    !sign_info(toks[i], &rank, &val) || rank >= prev_rank)
			return 0u;
		prev_rank = rank;
		sum += val;
	}
	return sum;
}

pn_status pn_greek_parse(const char *buf, size_t buflen, uint64_t *out)
{
	uint32_t toks[GREEK_MAX_TOKS];
	size_t n_tok = 0u, i = 0u, body, k;
	int low, sp_at = -1;
	uint64_t pre, suf, v;

	if (buf == NULL || out == NULL)
		return pn_err_null;
	if (buflen == 0u) {
			*out = 0u;                  /* the blank honors zero      */
		return pn_ok;
	}

	/*  Decode 2-byte signs only.  A lone byte under 0x80 (Latin,
	 *  NUL), an over-long run, a continuation without its lead, a
	 *  3-byte sequence or anything else canonical-but-foreign has
	 *  no table entry below — reject the shape here, the sign
	 *  table below rejects the stranger.                       */
	while (i < buflen) {
		unsigned char b = (unsigned char)buf[i];
		if (n_tok >= GREEK_MAX_TOKS)
			return pn_err_syntax;
		if (b == ' ') {
			toks[n_tok++] = GREEK_SPACE;
			i++;
		} else if (b < 0xC0u || b > 0xDFu || i + 2u > buflen ||
		           (buf[i + 1] & 0xC0) != 0x80) {
			return pn_err_syntax;
			} else {
			toks[n_tok++] = ((uint32_t)(b & 0x1Fu) << 6) |
			               (uint32_t)((unsigned char)buf[i + 1] & 0x3Fu);
			i += 2u;
			}
	}

	/*  the trailing keraia sits at the very end and nowhere else */
	if (toks[n_tok - 1u] != GREEK_DEXIA)
		return pn_err_syntax;
	body = n_tok - 1u;
	for (k = 0u; k < body; k++)
		if (toks[k] == GREEK_DEXIA)
			return pn_err_syntax;       /* a number in the middle     */
	if (body == 0u)
		return pn_err_syntax;           /* the lone horn-mark         */

	/*  the low keraia announces the cord from the front, once    */
	low = (toks[0] == GREEK_LOWK);
	i = low ? 1u : 0u;
	if (i >= body)
		return pn_err_syntax;           /* keraia around nothing      */
	for (k = i; k < body; k++)
		if (toks[k] == GREEK_LOWK)
			return pn_err_syntax;       /* a second announcement      */

	/*  the separator: at most one, touching neither edge, and
	  *  only under the low keraia — a cord below 1000 has no
	  *  second run and no gutter to spare.  sp_at keeps the
	  *  token index of the space, not an offset.                */
	for (k = i; k < body; k++) {
		if (toks[k] != GREEK_SPACE)
			continue;
		if (sp_at >= 0 || !low || k == i || k + 1u == body)
			return pn_err_syntax;
		sp_at = (int)k;
	}

	if (sp_at >= 0) {
		pre = run_value(&toks[i], (size_t)sp_at - i);
		suf = run_value(&toks[(size_t)sp_at + 1u], body - (size_t)sp_at - 1u);
		if (pre == 0u || suf == 0u)
			return pn_err_syntax;        /* a starved run is no run     */
		*out = pre * 1000u + suf;
		return pn_ok;
	}

	v = run_value(&toks[i], body - i);
	if (v == 0u)
		return pn_err_syntax;
	*out = low ? v * 1000u : v;
	return pn_ok;
}
