/*  brahmi.c — libPantheon: Brahmi numerals, UTF-8.

    Twenty signs, filling four slots from the top down - thousands,
    hundreds, tens, ones.  Unlike Egypt, no digit here is a
    repetition: every unit 1..9 and every ten 10..90 got its own
    dedicated sign, so 47 is two signs, not eleven:

    U+11052..U+1105A BRAHMI NUMBER ONE..NINE      = 1..9
    U+1105B..U+11063 BRAHMI NUMBER TEN..NINETY    = 10..90
    U+11064          BRAHMI NUMBER ONE HUNDRED    = 100
    U+11065          BRAHMI NUMBER ONE THOUSAND   = 1 000
    U+1107F          BRAHMI NUMBER JOINER        (the ligature join)

    Hundreds and thousands multiply: a digit 2..9 ligatured with its
    power sign - power first, then the joiner, then the multiplier -
    so 400 ligates into "hundred join four": one stacked sign for
    scribes to carve and readers to count.    An additive component
    stays unligated; 100 plus 4 (Unicode 14.x's ruling): the joiner
    marks what the two spellings mean apart and never drops out.    One
    multiplier needs no ligature: the power sign on its own already
    says 100.

    There is no sign above the thousand and no zero at all - a lone
    blank is the nothing owed - so 0 renders as the empty string, and
    anything past 9 999 is out of range rather than absurd.

    The parser reads the four slots, not the signs: 2 222 writes "two"
    three times - thousands multiplier, hundreds multiplier, ones term
    - and only a second term inside one slot is a blunder.  Two bare
    power signs in a row, a joiner with no multiplier behind it, a
    ligature left unligated (hundred four four: 100+4+4, and no slot
    allows the second four), or signs climbing up the pillar are all
    refused the way a reader refuses "IIIX".

    The signs are spelled as universal character names, which keeps
    this source pure ASCII.

    Reference: https://en.wikipedia.org/wiki/Brahmi_numerals
*/
#include <string.h>

#include "pantheon.h"

#define ONE       "\U00011052" /* = 1      UTF-8: F0 91 81 92 */
#define TWO       "\U00011053" /* = 2      UTF-8: F0 91 81 93 */
#define THREE     "\U00011054" /* = 3      UTF-8: F0 91 81 94 */
#define FOUR      "\U00011055" /* = 4      UTF-8: F0 91 81 95 */
#define FIVE      "\U00011056" /* = 5      UTF-8: F0 91 81 96 */
#define SIX       "\U00011057" /* = 6      UTF-8: F0 91 81 97 */
#define SEVEN     "\U00011058" /* = 7      UTF-8: F0 91 81 98 */
#define EIGHT     "\U00011059" /* = 8      UTF-8: F0 91 81 99 */
#define NINE      "\U0001105A" /* = 9      UTF-8: F0 91 81 9A */
#define TEN       "\U0001105B" /* = 10     UTF-8: F0 91 81 9B */
#define TWENTY    "\U0001105C" /* = 20     UTF-8: F0 91 81 9C */
#define THIRTY    "\U0001105D" /* = 30     UTF-8: F0 91 81 9D */
#define FORTY     "\U0001105E" /* = 40     UTF-8: F0 91 81 9E */
#define FIFTY     "\U0001105F" /* = 50     UTF-8: F0 91 81 9F */
#define SIXTY     "\U00011060" /* = 60     UTF-8: F0 91 81 A0 */
#define SEVENTY   "\U00011061" /* = 70     UTF-8: F0 91 81 A1 */
#define EIGHTY    "\U00011062" /* = 80     UTF-8: F0 91 81 A2 */
#define NINETY    "\U00011063" /* = 90     UTF-8: F0 91 81 A3 */
#define HUNDRED   "\U00011064" /* = 100    UTF-8: F0 91 81 A4 */
#define THOUSAND  "\U00011065" /* = 1 000  UTF-8: F0 91 81 A5 */
#define JOINER    "\U0001107F" /* the ligature join, for numbers only */
#define SIGN_BYTES 4                   /* every sign is astral: 4 bytes of UTF-8 */

static const char *const units[9] = {
	ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE
};

static const char *const tens[9] = {
	TEN, TWENTY, THIRTY, FORTY, FIFTY, SIXTY, SEVENTY, EIGHTY, NINETY
};

/* Write @param n's canonical Brahmi string; @param buf may be NULL,
   which only counts.  Past PN_BRAHMI_MAX_VALUE the count drops the
   places above the thousand and its answer means nothing - render
   turns it down. */
static size_t brahmi_emit(uint64_t n, char *buf)
{
	size_t bytes = 0;

#define PUT(s) do {                                                            \
		if (buf != NULL)                                                       \
			memcpy(buf + bytes, (s), SIGN_BYTES);                              \
		bytes += SIGN_BYTES;                                                   \
	} while (0)

	/* One power term: the power sign alone for digit 1, ligatured as
	   <power JOINER units-digit> for digits 2..9, and silent for 0. */
#define PUT_POWER(power, d) do {                                               \
		if ((d) >= 1u)                                                         \
			PUT(power);                                                        \
		if ((d) >= 2u) {                                                       \
			PUT(JOINER);                                                       \
			PUT(units[(d) - 1u]);                                              \
		}                                                                      \
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
	/* This answer never fails: it is exactly what brahmi_emit
	 *counts, which is what render will write. */
	return brahmi_emit(n, NULL);
}

bool pn_brahmi_render(uint64_t n, char *_Nonnull buf, size_t buflen,
                      pn_error *_Nullable *_Nullable error)
{
	if (buf == NULL)
		return pn_error_set(error, PN_ERROR_NULL,
				                           "render needs a buffer to write into");
	if (n > PN_BRAHMI_MAX_VALUE)
		return pn_error_set(error, PN_ERROR_RANGE,
				                           "%llu runs past %llu: no sign"
				                           " above the thousand exists"
				                           " (ten thousand is words,"
				                           " not signs)",
				                           (unsigned long long)n,
				                           (unsigned long long)PN_BRAHMI_MAX_VALUE);

	const size_t need = brahmi_emit(n, NULL);   /*exact: buflen must hold +NUL */

	if (buflen <= need)
		return pn_error_set(error, PN_ERROR_BUFFER,
				                           "buffer holds %zu bytes; this"
				                           " string needs %zu plus the"
				                           " terminating NUL",
				                          buflen, need);
	(void)brahmi_emit(n, buf);
	buf[need] = '\0';          /* n == 0: just the blank, and the NUL */
	return true;
}

/* Parsing reads back what rendering wrote.

   The canonical form is what makes the round trip exact: the twenty
   number signs plus the joiner, read as at most four terms climbing
   down - thousands, hundreds, tens, ones - each term at most once,
   with nothing between them.  A hundreds or thousands term is either
   the bare power sign (digit 1) or the ligature <power JOINER
   units-digit> (digits 2..9); the ones and tens terms are their one
   dedicated sign.  Everything else - foreign bytes, a sign cut off
   mid-UTF-8, a term out of its slot, a joiner joining anything but a
   multiplier to its power, a 1-times-100 ligature that should have
   been the bare sign, or an unligated ligature like "hundred four
   four" - is refused the way a reader refuses "IIIX". */

/* The full sign inventory in code-point order: units, tens, powers,
   joiner - the joiner is never a term and never leads a match. */
#define N_SIGNS 21
static const char *const signs[N_SIGNS] = {
	ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE,
	TEN, TWENTY, THIRTY, FORTY, FIFTY, SIXTY, SEVENTY, EIGHTY, NINETY,
	HUNDRED, THOUSAND, JOINER
};
#define SIG_NINE       8
#define SIG_TEN         9
#define SIG_NINETY      17
#define SIG_HUNDRED     18
#define SIG_THOUSAND    19
#define SIG_JOINER      20

/* Match one whole sign at @param p: its index in the table above, or
   -1 for a foreign byte or a sign cut off short. */
static int match_sign(const char *p, const char *end)
{
	for (int i = 0; i < N_SIGNS; i++)
		if (p + SIGN_BYTES <= end && memcmp(p, signs[i], SIGN_BYTES) == 0)
			return i;
	return -1;
}

bool pn_brahmi_parse(const char *_Nullable buf, size_t buflen,
                     uint64_t *_Nullable out,
                     pn_error *_Nullable *_Nullable error)
{
	if (buf == NULL || out == NULL)
		return pn_error_set(error, PN_ERROR_NULL,
				                           "parse needs a string and a place"
				                           " to put the number");
	if (buflen == 0) {
				*out = 0;                  /* the blank reads back as the nothing it was */
		return true;
	}

	uint64_t sum = 0;     /* built here; *out keeps its old value unless all works */
	int slot = 0;         /* the lowest term not yet spoken: 0 thousands .. 3 ones */
	const char *p = buf, *end = buf + buflen;

	while (p < end) {
		const int s = match_sign(p, end);

		if (s < 0)
			return pn_error_set(error, PN_ERROR_SYNTAX,
				                             "position %zu holds a foreign"
				                             " byte or a sign cut off short",
				                             (size_t)(p - buf));
		if (s == SIG_JOINER)
			return pn_error_set(error, PN_ERROR_SYNTAX,
				                             "a joiner at position %zu with"
				                             " nothing being joined to it"
				                             " (doubled, leading, or off its"
				                             " power sign)",
				                             (size_t)(p - buf));
		p += SIGN_BYTES;

		uint64_t worth;
		int next;                   /* the slot the next term must clear */

		if (s == SIG_THOUSAND || s == SIG_HUNDRED) {
			const int is_thousand = (s == SIG_THOUSAND);
			const uint64_t power = is_thousand ? 1000u : 100u;

			if (slot > (is_thousand ? 0 : 1))
				return pn_error_set(error, PN_ERROR_SYNTAX,
				                                 "a second term of this"
				                                 " power, or an ascending"
				                                 " one: the pillar walks"
				                                 " down from the thousand");

			/* The digit: the bare power sign means 1; the ligature
			   <JOINER units-digit> multiplies it. */
			worth = power;
			if (p + SIGN_BYTES <= end &&
					memcmp(p, JOINER, SIGN_BYTES) == 0) {
				p += SIGN_BYTES;
				const int c = match_sign(p, end);

				if (c < 0 || c > SIG_NINE)
					return pn_error_set(error, PN_ERROR_SYNTAX,
					                             "the joiner at position"
					                             " %zu joins to a"
					                             " non-multiplier (a tens"
					                             " sign, a power, a second"
					                             " joiner) or to nothing",
					                             (size_t)(p - buf) - SIGN_BYTES);
				if (c == 0)
					return pn_error_set(error, PN_ERROR_SYNTAX,
					                             "1 times the power: write"
					                             " the power sign bare,"
					                             " not ligatured");
				worth = (uint64_t)(c + 1) * power;
				p += SIGN_BYTES;
						}
			next = is_thousand ? 1 : 2;
				} else if (s >= SIG_TEN && s <= SIG_NINETY) {
			if (slot > 2)
				return pn_error_set(error, PN_ERROR_SYNTAX,
				                             "a tens term twice, or after"
				                             " the ones term");
			worth = (uint64_t)(s - SIG_TEN + 1) * 10u;
			next = 3;
				} else {                /* a bare units sign: the ones term */
			if (slot > 3)
				return pn_error_set(error, PN_ERROR_SYNTAX,
				                             "the ones term is already"
				                             " spoken");
			worth = (uint64_t)(s + 1);
			next = 4;
				}

		/* No overflow possible: four slots filled to the brim are
		   9 000 + 900 + 90 + 9 = PN_BRAHMI_MAX_VALUE. */
		sum += worth;
		slot = next;
			}
	*out = sum;
	return true;
}
