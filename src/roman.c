/*  roman.c — libPantheon: Roman numerals, the canon tally, pure
    ASCII.

    The same direct-to-uint64 rule book as the egyptian scribe and
    the quipu cord: an additive notation has no places to hold, so no
    struct layer sits between the number and its letters.

    The grammar: four rows per digit, each written out in full - the
    thousands run (up to three Ms), the hundreds row, the tens row,
    the units row.    The rows descend; each run of I, X, C or M holds at
    most three; and the subtractive pairs IV IX XL XC CD CM stand
    once inside their own digit.  This is the abacist's settled
    canon: no clock-maker's IIII, no poet's IC, nothing past MMM CM
    XC IX.

    The parser walks the greedy canon table - each step takes the
    largest token the remaining letters still allow to start with.
    Better still, every numeral it accepts goes back through the
    renderer: a string that reads to @param n must render out byte
    for byte as itself, the exact round trip the house inverse law
    demands.  Misspellings (IIII, IL, VX, XCX, lowercase letters,
    spaces, foreign bytes) either find no token where the walk
    demands one, or fail the read-back.  No run bookkeeping can
    disagree with the renderer, because the renderer is the arbiter.

    References: https://en.wikipedia.org/wiki/Roman_numerals
*/
#include <string.h>

#include "pantheon.h"

/* The four rows of tallies, spelled out.    The index inside a row is
   that digit (the thousands row: 0..3 only). */
static const char *const th_row[4] = { "", "M", "MM", "MMM" };
static const char *const h_row[10] = { "",        "C",      "CC",     "CCC",
	                                       "CD",      "D",      "DC",     "DCC",
	                                       "DCCC",    "CM" };
static const char *const t_row[10] = { "",        "X",      "XX",     "XXX",
	                                       "XL",      "L",      "LX",     "LXX",
	                                       "LXXX",    "XC" };
static const char *const u_row[10] = { "",        "I",      "II",     "III",
	                                       "IV",      "V",      "VI",     "VII",
	                                       "VIII",    "IX" };

size_t pn_roman_render_size(uint64_t n)
{
	/* This answer never fails.  Past 1..PN_ROMAN_MAX_VALUE it answers
	 *0 — the empty tally — because turning down the number is
	  *render's job. */
	if (n == 0u || n > PN_ROMAN_MAX_VALUE)
		return 0u;
	return strlen(th_row[n / 1000u]) + strlen(h_row[(n / 100u) % 10u]) +
	       strlen(t_row[(n / 10u) % 10u]) + strlen(u_row[n % 10u]);
}

/* Stick one row's letters onto the tally at @w. */
static void put_row(const char *row, char *buf, size_t *w)
{
	while (*row != '\0')
		buf[(*w)++] = *row++;
}

bool pn_roman_render(uint64_t n, char *_Nonnull buf, size_t buflen,
                     pn_error *_Nullable *_Nullable error)
{
	if (buf == NULL)
		return pn_error_set(error, PN_ERROR_NULL,
				                       "render needs a buffer to write into");
	if (n > PN_ROMAN_MAX_VALUE)
		return pn_error_set(error, PN_ERROR_RANGE,
				                       "%llu runs past %llu: the tally"
				                       " walks no higher than MMM CM XC"
				                       " IX",
				                      (unsigned long long)n,
				                      (unsigned long long)PN_ROMAN_MAX_VALUE);
	if (buflen < pn_roman_render_size(n) + 1u)
		return pn_error_set(error, PN_ERROR_BUFFER,
				                       "buffer holds %zu byte(s); this"
				                       " numeral needs %zu plus a"
				                       " terminating 0 byte",
				                    buflen, pn_roman_render_size(n));

	size_t w = 0u;

	put_row(th_row[n / 1000u], buf, &w);
	put_row(h_row[(n / 100u) % 10u], buf, &w);
	put_row(t_row[(n / 10u) % 10u], buf, &w);
	put_row(u_row[n % 10u], buf, &w);
	buf[w] = '\0';
	return true;
}

/* The greedy walk's table: how much a token is worth and how it
   reads, largest first - the very order the renderer's rows write
   out. */
static const struct {
	uint64_t value;
	char text[3];
} canon[] = {
	{ 1000u, "M"   }, { 900u, "CM" }, { 500u, "D"   },
	{  400u, "CD"  }, { 100u, "C"   }, {  90u, "XC"  },
	{   50u, "L"  }, {  40u, "XL"  }, {  10u, "X"   },
	{    9u, "IX"  }, {   5u, "V"   }, {   4u, "IV"  },
	{    1u, "I"  },
};
#define N_CANON ((sizeof canon) / (sizeof canon[0]))

bool pn_roman_parse(const char *_Nullable buf, size_t buflen,
                    uint64_t *_Nullable out,
                    pn_error *_Nullable *_Nullable error)
{
	if (buf == NULL || out == NULL)
		return pn_error_set(error, PN_ERROR_NULL,
				                       "parse needs a string and a place to"
				                       " put the number");
	if (buflen == 0u) {
			 *out = 0u;                    /* Bede's nulla: the blank tally  */
		return true;
		}

	/* Greedy walk: at each step take the biggest canon token that
	  *the rest of the string still starts with.    A byte the table has
	  *no token for — a NUL, a space, a lowercase letter, any non-ASCII
	  *byte — is the syntax error the abacist's scribe would mark. */
	uint64_t sum = 0u;
	size_t i = 0u;

	while (i < buflen) {
		int matched = 0;

		for (unsigned k = 0u; k < N_CANON; k++) {
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
			return pn_error_set(error, PN_ERROR_SYNTAX,
				                           "position %zu starts no"
				                           " Roman token: a run of"
				                           " letters the canon does"
				                           " not spell ends the walk",
				                         i);
		}

	/* The read-back is the real test: what parses has to render back
	  *to itself byte for byte, so no un-canonical tally (IIII, VIV,
	  *XCX, MDCCCCLXXXXVIIII) sneaks through on a sum that happens to
	  *land in range.    pn_roman_render refuses n == 0 with an empty
	  *string; the guard above keeps that case out, so only a true
	  *misspelling fails here. */
	char back[PN_ROMAN_FORMAT_MAX_LEN];
	pn_error *inner = NULL;
	bool ok = pn_roman_render(sum, back, sizeof back, &inner);

	pn_error_free(inner);      /* its verdict is already ours */
	if (!ok || strlen(back) != buflen || memcmp(back, buf, buflen) != 0)
		return pn_error_set(error, PN_ERROR_SYNTAX,
				                       "\"%.*s\" does not spell the"
				                       " canon for %llu: the tally"
				                       " must render back to itself",
				                     (int)(buflen > 64 ? 64 : buflen), buf,
				                    (unsigned long long)sum);

	*out = sum;
	return true;
}
