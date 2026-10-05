/* roman.c — Roman numerals, the canonical tallied form, pure ASCII.
 *
 * The uint64-direct rule book, same as the egyptian scribe and the
 * quipu cord: an additive notation has no places to hold, so no
 * struct layer sits between the number and its letters.
 *
 * The grammar: four per-digit rows written in full — the thousands
 * run (up to three M), the hundreds row, the tens row, the units
 * row.  The rows descend, each run of I, X, C or M holds at most
 * three, the subtractive digraphs IV IX XL XC CD CM stand once in
 * their own digit: the abacist's settled canon, no clock-maker IIII,
 * no poet's IC, no vinculum past MMM CM XC IX.
 *
 * The parser is the greedy canon walk — at each step take the only
 * token the next digit may open with — and every accepted numeral is
 * read back through the renderer: a string that parses to n must
 * render byte-for-byte back to itself, the exact bijection the house
 * inverse law demands.  Misspellings (IIII, IL, VX, XCX, lowercase,
 * spaces, foreign bytes) either find no token where one is demanded
 * or fail the read-back; no run bookkeeping can disagree with the
 * renderer because the renderer is the arbiter.
 */

#include <string.h>

#include "pantheon.h"

/*  The per-digit rows; index is the digit (thousands: 0..3 only).  */
static const char *const th_row[4] = { "", "M", "MM", "MMM" };
static const char *const h_row[10] = { "",       "C",    "CC",   "CCC",
	                                        "CD",     "D",    "DC",   "DCC",
	                                        "DCCC",   "CM" };
static const char *const t_row[10] = { "",     "X",    "XX",   "XXX",
	                                        "XL",     "L",    "LX",   "LXX",
	                                        "LXXX",   "XC" };
static const char *const u_row[10] = { "",    "I",    "II",   "III",
	                                        "IV",     "V",    "VI",   "VII",
	                                        "VIII",   "IX" };

size_t pn_roman_render_size(uint64_t n)
{
	if (n == 0u || n > PN_ROMAN_MAX_VALUE)
		return 0u; /* the blank cord; render owns the range verdict */
	return strlen(th_row[n / 1000u]) + strlen(h_row[(n / 100u) % 10u]) +
	       strlen(t_row[(n / 10u) % 10u]) + strlen(u_row[n % 10u]);
}

/*  Append `row` to buf at *w. */
static void put_row(const char *row, char *buf, size_t *w)
{
	while (*row != '\0')
		buf[(*w)++] = *row++;
}

pn_status pn_roman_render(uint64_t n, char *buf, size_t buflen)
{
	size_t w = 0u;

	if (buf == NULL)
		return pn_err_null;
	if (n > PN_ROMAN_MAX_VALUE)
		return pn_err_range;
	if (buflen < pn_roman_render_size(n) + 1u)
		return pn_err_buffer;
	put_row(th_row[n / 1000u], buf, &w);
	put_row(h_row[(n / 100u) % 10u], buf, &w);
	put_row(t_row[(n / 10u) % 10u], buf, &w);
	put_row(u_row[n % 10u], buf, &w);
	buf[w] = '\0';
	return pn_ok;
}

/*  The greedy canon walk's table: value and spelling, descending —
 *  the very order the renderer's rows produce. */
static const struct {
	uint64_t value;
	char text[3];
} canon[] = {
	{ 1000u, "M"  }, { 900u, "CM" }, { 500u, "D"  },
	{ 400u,  "CD" }, { 100u, "C"  }, { 90u,  "XC" },
	{ 50u,      "L"  }, { 40u,  "XL" }, { 10u,  "X"  },
	{ 9u,       "IX" }, { 5u,   "V"  }, { 4u,   "IV" },
	{ 1u,        "I"  },
};
#define N_CANON ((sizeof canon) / (sizeof canon[0]))

pn_status pn_roman_parse(const char *buf, size_t buflen, uint64_t *out)
{
	char back[PN_ROMAN_FORMAT_MAX_LEN];
	uint64_t sum = 0u;
	size_t i = 0u;
	unsigned k;
	int matched;

	if (buf == NULL || out == NULL)
		return pn_err_null;
	if (buflen == 0u) {
			*out = 0u;                  /* Bede's nulla, the blank cord */
		return pn_ok;
	}

	/*  Greedy: at each step take the largest canon token that fits
	  *  the cord; a byte the table has no token for — a NUL, a
	  *  space, a lowercase letter, any non-ASCII — is the syntax
	  *  error the abacist's scribe would mark.                   */
	while (i < buflen) {
		matched = 0;
		for (k = 0u; k < N_CANON; k++) {
			size_t len = strlen(canon[k].text);
			if (i + len <= buflen &&
			     memcmp(buf + i, canon[k].text, len) == 0) {
				sum += canon[k].value;
				i += len;
				matched = 1;
					break;
					}
				}
		if (!matched)
			return pn_err_syntax;           /* no token lives at i       */
	}

	/*  the read-back: what parses must render back to itself, so no
	 *  un-canonical tally (IIII, VIV, XCX, MDCCCCLXXXXVIIII) slips
	 *  through on a sum that happens to land in range.          */
	if (pn_roman_render(sum, back, sizeof back) != pn_ok ||
	     strlen(back) != buflen || memcmp(back, buf, buflen) != 0)
		return pn_err_syntax;

			*out = sum;
	return pn_ok;
}
