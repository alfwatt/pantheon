/*  chinese.c — libPantheon: hanzi numerals, UTF-8.
   	*
   	*  Decimal with multipliers instead of place-value digits: the value
   	*  is A * 10^4 + B, and each section writes its nonzero digits high
   	*  to low as (digit + 千/百/十), the section's units digit bare and
   	*  last:
   	*
   	*    四千六百二十二 = 4622   (Wikipedia's worked example)
   	*    五千万 五       = 50 000 005
   	*
   	*  The blank for zero follows the Egyptian ruling: a skipped digit is
   	*  not written at all, and the skip is acknowledged by exactly one
   	*  space between the surrounding terms — 一千 五 = 1005, 一千 五十 =
   	*  1050, where later scribes would write 零.  The space carries no
   	*  value (multipliers pin every digit's place), so it is a pure
   	*  canonical marker and the round trip is exact.  Canonical form is
   	*  the full formal reading: 一十四, never the spoken elision 十四.
   	*  The domain stops at 万 — no sign above it here, as Egypt had none
   	*  above the Heh — so PN_HANZI_MAX_VALUE is 99 999 999.
   	*
   	*  The signs are written as universal character names so this source
   	*  stays pure ASCII.
   	*
   	*  Reference: https://en.wikipedia.org/wiki/Chinese_numerals
   	*/

#include <string.h>

#include "pantheon.h"

#define YI   "\U00004E00" /* 一 = 1    UTF-8: E4 B8 80 */
#define ER   "\U00004E8C" /* 二 = 2    UTF-8: E4 BA 8C */
#define SAN  "\U00004E09" /* 三 = 3    UTF-8: E4 B8 89 */
#define SI   "\U000056DB" /* 四 = 4    UTF-8: E5 9B 9B */
#define WU   "\U00004E94" /* 五 = 5    UTF-8: E4 BA 94 */
#define LIU  "\U0000516D" /* 六 = 6    UTF-8: E5 85 AD */
#define QI   "\U00004E03" /* 七 = 7    UTF-8: E4 B8 83 */
#define BA   "\U0000516B" /* 八 = 8    UTF-8: E5 85 AB */
#define JIU  "\U00004E5D" /* 九 = 9    UTF-8: E4 B9 9D */
#define SHI  "\U00005341" /* 十 = 10   UTF-8: E5 8D 81 */
#define BAI  "\U0000767E" /* 百 = 100  UTF-8: E7 99 BE */
#define QIAN "\U00005343" /* 千 = 1k   UTF-8: E5 8D 83 */
#define WAN  "\U00004E07" /* 万 = 10k  UTF-8: E4 B8 87 */
#define SIGN_BYTES 3                /* every sign is BMP: 3 bytes of UTF-8 */

static const char *const digits[9] = {
	YI, ER, SAN, SI, WU, LIU, QI, BA, JIU
};

/* Multipliers of a section, in descending order: place 3, 2, 1. */
static const char *const mults[3] = { QIAN, BAI, SHI };

/* The value of a digit at place k (0 = units) inside its section. */
static const uint64_t pow10s[4] = { 1u, 10u, 100u, 1000u };

/* Emit n's canonical hanzi string; buf may be NULL, which merely counts.
  * Pure and total: n > PN_HANZI_MAX_VALUE simply drops the places above
  * 万 and the answer is meaningless — render rejects such n with
  * pn_err_range. */
static size_t hanzi_emit(uint64_t n, char *buf)
{
	size_t bytes = 0;
	const uint64_t secs[2] = { n / 10000u, n % 10000u };
	int prev = -1; /* absolute place of the last emitted term */

#define PUT(s, len) do {                                                   \
		if (buf != NULL)                                                   \
			memcpy(buf + bytes, (s), (size_t)(len));                   \
		bytes += (size_t)(len);                                            \
	} while (0)

	for (int s = 0; s < 2; s++) {
		const uint64_t sec = secs[s];
		const int base = s == 0 ? 4 : 0;
		for (int k = 3; k >= 0; k--) {
			const unsigned d = (unsigned)((sec / pow10s[k]) % 10u);
			if (d == 0u)
				continue;         /* zero: not written, just acknowledged */
			const int place = base + k;
			if (prev >= 0 && prev - place > 1)
				PUT(" ", 1u);     /* the blank: 零's predecessor */
			PUT(digits[d - 1u], SIGN_BYTES);
			if (k > 0)
				PUT(mults[3 - k], SIGN_BYTES);
			prev = place;
		}
		if (s == 0 && sec != 0u)
			PUT(WAN, SIGN_BYTES); /* 万 rides the top section's last term */
	}

#undef PUT
	return bytes;
}

size_t pn_hanzi_render_size(uint64_t n)
{
	return hanzi_emit(n, NULL);
}

pn_status pn_hanzi_render(uint64_t n, char *buf, size_t buflen)
{
	if (buf == NULL)
		return pn_err_null;
	if (n > PN_HANZI_MAX_VALUE)
		return pn_err_range;    /* no sign above 万: 亿 is not written here */

	const size_t need = hanzi_emit(n, NULL); /* exact: buflen must fit it + NUL */
	if (buflen <= need)
		return pn_err_buffer;

	(void)hanzi_emit(n, buf);
	buf[need] = '\0';                       /* n == 0: just the blank, and the NUL */
	return pn_ok;
}

/* --- parsing ------------------------------------------------------------
  	*
  	* Canonical form is what makes the round trip exact.  A string is
  	* terms — (digit [千|百|十]) or a bare digit — strictly descending in
  	* place within each section, at most one 万 written immediately
  	* after the top section's last term.  Between consecutive terms a
  	* space appears iff they are more than one place apart; adjacent
  	* terms must touch.  Anything else — foreign bytes, a cut-off sign,
  	* 十四 without its 一, 四百 三十一 with an unjustified space,
  	* 五千万五 missing its blank — is rejected the way a reader rejects
  	* "IIIX".
  	*/

/* Match a digit sign at p: return 1..9, or -1. */
static int match_digit(const char *p, const char *end)
{
	for (int i = 0; i < 9; i++)
		if (p + SIGN_BYTES <= end && memcmp(p, digits[i], SIGN_BYTES) == 0)
			return i + 1;
	return -1;
}

/* Match a section multiplier at p: 3/2/1 for 千/百/十, 0 for none.
   * 万 is not a section multiplier — the string is split at it first. */
static int match_mult(const char *p, const char *end)
{
	if (p + SIGN_BYTES > end)
		return 0;
	if (memcmp(p, QIAN, SIGN_BYTES) == 0)
		return 3;
	if (memcmp(p, BAI, SIGN_BYTES) == 0)
		return 2;
	if (memcmp(p, SHI, SIGN_BYTES) == 0)
		return 1;
	return 0;
}

pn_status pn_hanzi_parse(const char *buf, size_t buflen, uint64_t *out)
{
	if (buf == NULL || out == NULL)
		return pn_err_null;
	if (buflen == 0) {
		*out = 0;                   /* the blank reads back as the nothing it was */
		return pn_ok;
	}
	if (buf[0] == ' ' || buf[buflen - 1] == ' ')
		return pn_err_syntax;        /* the blank sits between terms, never at a border */

	const char *end = buf + buflen;

	/* Split at 万 first: at most one, and written immediately after
	   * the top section's last term — so never at either border, and
	   * no 万 means no top section at all.  E4 can only ever be a
	   * sign's lead byte (continuations are 80..BF), so a byte scan
	   * finds it exactly where a reader sees it. */
	const char *wan = NULL;
	for (const char *w = buf; w + SIGN_BYTES <= end; w++)
		if (memcmp(w, WAN, SIGN_BYTES) == 0) {
			if (wan != NULL)
				return pn_err_syntax;    /* a second 万 */
			wan = w;
		}
	if (wan != NULL && wan == buf)
		return pn_err_syntax;              /* 万 with no top section to ride */

	const char *const starts[2] = { buf, wan != NULL ? wan + SIGN_BYTES : buf };
	const char *const stops[2] = { wan != NULL ? wan : buf, end };
	const int bases[2] = { 4, 0 };       /* the 万-section rides four places up */

	uint64_t secs[2] = { 0u, 0u };   /* build locally; *out untouched unless we succeed */
	int prev_k[2] = { 4, 4 };        /* last term's place per section: strict descent */
	int prev = -1;                   /* absolute place of the last term, for spacing */

	for (int s = 0; s < 2; s++) {
		const char *p = starts[s];
		while (p < stops[s]) {
			int run = 0;             /* spaces before this term */
			while (p + (size_t)run < stops[s] && p[run] == ' ')
				run++;

			const int d = match_digit(p + run, stops[s]);
			if (d < 0)
				return pn_err_syntax;/* foreign byte, cut-off sign, or a
			                          * space where a digit belongs */
			const char *q = p + run + SIGN_BYTES;

			int k = match_mult(q, stops[s]); /* 3/2/1 = the term's place, else bare units */
			if (k >= 1 && k <= 3)
				q += SIGN_BYTES;
			else
				k = 0;
			const int place = bases[s] + k;

			if (k >= prev_k[s])
				return pn_err_syntax;/* ascending or repeated multiplier, */
			prev_k[s] = k;           /* or any digit after a section's bare one */

			const int gap = prev - place;
			if (prev < 0) {
				if (run != 0)
					return pn_err_syntax;  /* leading spaces */
			} else if (gap <= 0) {
				return pn_err_syntax;      /* terms cannot go backwards in place */
			} else if (gap == 1 && run != 0) {
				return pn_err_syntax;      /* a space where adjacent terms meet */
			} else if (gap > 1 && run != 1) {
				return pn_err_syntax;      /* a skip unacknowledged, or doubled */
			}

			secs[s] += (uint64_t)d * pow10s[k];
			prev = place;
			p = q;
		}
	}

	/* The grammar admits no value past PN_HANZI_MAX_VALUE: sections
	   * top out at 9999, so no overflow check is needed. */
	*out = secs[0] * 10000u + secs[1];
	return pn_ok;
}
