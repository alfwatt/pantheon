/*	quipu.c — libPantheon: Inca quipu knot notation, UTF-8.
 *
 *	Positional decimal along a pendant cord: the units line nearest
 *	the free end, powers of ten marching toward the main cord (the
 *	quipucamayuq read bottom-up; this transcription lays the cord
 *	down most-significant-first, the linearisation rods.c gives the
 *	counting board).  Unicode encodes no quipu signs — no knot block
 *	to borrow, unlike cuneiform's wedges in babylonian.c — so two
 *	generic BMP glyphs transcribe (three UTF-8 bytes each):
 *
 *	  U+2022 BULLET    E2 80 A2   d of them = d simple knots (Locke's
 *	                                "d s") in every line above the units
 *	  U+221E INFINITY  E2 88 9E   d of them = the long knot of d turns
 *	                               at the units line ("d L"); just one
 *	                               of them is the figure-eight (Locke's
 *	                               "E") — a long knot of one turn
 *	                               cannot be tied
 *
 *	The units line is drawn in the figure-eight family ONLY, every
 *	line above it in simple knots ONLY: Locke's distinctive units
 *	knot becomes this grammar's form-must-match-place rule, the knot
 *	answer to the rods' alternating sets.  Zero is the ABSENCE of a
 *	knot in the right position — no sign for it exists, none is
 *	invented: a blank place writes no knots, every place boundary
 *	writes exactly one space, and a well-formed cord holds exactly
 *	(places - 1) spaces, from which the parser recovers the geometry
 *	unaided: places - 1 spaces means places - 1 place steps, so every
 *	column ends its run exactly where the next boundary begins.  Ten
 *	identical knots is more turns than any digit holds (range, as the
 *	tenth DISH falls under at babylonian.c); a twentieth column is
 *	more cord than a uint64_t holds.  The knots arrive as universal
 *	character names via the header's PN_QUIPU_* signs so this source
 *	stays pure ASCII.
 *
 *	References: https://en.wikipedia.org/wiki/Quipu ,
 *	Locke, The Ancient Quipu or Peruvian Knot Record (1912);
 *	Ascher & Ascher, Mathematics of the Incas: Code of the Quipu (1997).
 */

#include <string.h>

#include "pantheon.h"

#define SIMPLE_PREFIX   "\u2022"         /*whole bullet:   E2 80 A2        */
#define FIGURE_PREFIX   "\u221e"         /*whole infinity: E2 88 9E        */
#define SIGN_BYTES      3                /*both signs are BMP, 3 UTF-8 bytes */

/*	10^0 through 10^(PN_QUIPU_MAX_PLACES - 1): the weight of every
 *	line a pendant holds on a cord a uint64_t can carry. */
static const uint64_t pow10[PN_QUIPU_MAX_PLACES] = {
	1ull, 10ull, 100ull, 1000ull, 10000ull, 100000ull, 1000000ull,
	10000000ull, 100000000ull, 1000000000ull, 10000000000ull,
	100000000000ull, 1000000000000ull, 10000000000000ull,
	100000000000000ull, 1000000000000000ull, 10000000000000000ull,
	100000000000000000ull, 1000000000000000000ull
};

/*	Which knot sits at p, which has at least SIGN_BYTES room before
 *	end: 0 the simple knot, 1 the figure-eight family, -1 anything
 *	else.  Whole knots or nothing: near misses (U+2023 TOP BULLET,
 *	U+221D PROPORTIONAL TO, U+00B7 MIDDLE DOT, the Mayan, Brahmi and
 *	rod glyphs) all fall through, and so does a knot cut off
 *	mid-UTF-8 — the room check says its bytes are no knot. */
static int quipu_match_knot(const char *p, const char *end)
{
	if (p + SIGN_BYTES > end)
		return -1;
	if (memcmp(p, SIMPLE_PREFIX, SIGN_BYTES) == 0)
		return 0;
	if (memcmp(p, FIGURE_PREFIX, SIGN_BYTES) == 0)
		return 1;
	return -1;
}

/*	The shared emit/count helper: buf == NULL counts bytes, else
 *	writes them — never including the terminating NUL.  +1 place:
 *	the count keeps running one digit past the domain, for answers
 *	above PN_QUIPU_MAX_VALUE that are meaningless but never fatal —
 *	render rejects such n with pn_err_range before ever writing. */
static size_t quipu_emit(uint64_t n, char *buf)
{
	uint8_t dig[PN_QUIPU_MAX_PLACES + 2];
	int m = 0;

	do {                                  /*least significant place first */
		dig[m++] = (uint8_t)(n % 10u);
		n /= 10u;
	} while (n != 0);

	size_t bytes = 0;
	for (int i = m - 1; i >= 0; i--) {               /*top of the pendant */
		if (i != m - 1) {
			if (buf != NULL)
				buf[bytes] = ' ';           /*one boundary per place step */
			bytes += 1u;
		}
		const char *sign = (i == 0) ? FIGURE_PREFIX : SIMPLE_PREFIX;
		for (size_t k = 0; k < (size_t)dig[i]; k++) {  /*whole knots only */
			if (buf != NULL)
				memcpy(buf + bytes + k * SIGN_BYTES, sign, SIGN_BYTES);
		}
		bytes += (size_t)dig[i] * SIGN_BYTES;   /*a blank place adds none */
	}
	return bytes;
}

size_t pn_quipu_render_size(uint64_t n)
{
	/*	Pure and total: exactly what quipu_emit writes.  Past
	 *	PN_QUIPU_MAX_VALUE the count runs on and the answer is
	 *	meaningless — render rejects such n with pn_err_range. */
	return quipu_emit(n, NULL);
}

pn_status pn_quipu_render(uint64_t n, char *buf, size_t buflen)
{
	if (buf == NULL)
		return pn_err_null;
	if (n > PN_QUIPU_MAX_VALUE)
		return pn_err_range;        /*a twentieth line: 10^19 no longer fits */

	const size_t need = quipu_emit(n, NULL);    /*exact: buflen must hold +NUL */
	if (buflen <= need)
		return pn_err_buffer;

	(void)quipu_emit(n, buf);
	buf[need] = '\0';
	return pn_ok;
}

/*	parsing -----------------------------------------------------------------
 *
 *	The whole cord is knots and boundary spaces in canonical spacing.
 *	The space count pins the geometry (places = spaces + 1, the rods'
 *	law) and the column's OWN place decides which knot family is
 *	legal in it — captured the moment the column opens, so trailing
 *	boundaries can never frame an innocent knot.  A column must also
 *	END exactly where its last boundary begins: that check is what
 *	makes "well-formed" mean it — an interior gutter would end a
 *	column a place too early, and the topmost column may not sit
 *	under a gutter either, since the first space always steps down.
 *	Ten of any knot overfills a digit long before the cord can
 *	overfill a uint64_t: the babylonian tenth-DISH ruling reigned
 *	from syntax to range on the knot count, while the twentieth place
 *	stays the page-overrun range call.
 */

pn_status pn_quipu_parse(const char *buf, size_t buflen, uint64_t *out)
{
	if (buf == NULL || out == NULL)
		return pn_err_null;
	if (buflen == 0) {
		*out = 0;                     /*the empty cord: no knot was tied   */
		return pn_ok;
	}
	if (buf[0] == ' ')
		return pn_err_syntax;              /*the cord never starts in a gutter */

	size_t spaces = 0;                   /*counted first: it defines places  */
	for (size_t i = 0; i < buflen; i++)
		if (buf[i] == ' ')
			spaces++;
	if (spaces + 1u > (size_t)PN_QUIPU_MAX_PLACES)
		return pn_err_range;               /*more cord than a uint64_t holds   */

	uint64_t sum = 0;                      /**out untouched unless we win        */
	int place = (int)spaces;          /*topmost line; steps down each gutter    */
	int col_kind = -1;                  /*the column being tied                  */
	int col_count = 0;
	int col_place = -1;               /*where it OPENED — the anchor         */
	const char *p = buf;
	const char *end = buf + buflen;

	while (p < end) {
		if (*p == ' ') {
			if (col_count != 0) {            /*a boundary ends this column   */
				if (col_place != place       /*gutter inside the run, or     */
					 || (col_kind == 1) != (place == 0))
					return pn_err_syntax;    /*wrong line for these knots    */
				sum += (uint64_t)col_count * pow10[place];
				col_count = 0;
				col_kind = -1;
				col_place = -1;
			}
			place--;
			p++;
			continue;
		}
		const int kind = quipu_match_knot(p, end);
		if (kind < 0)
			return pn_err_syntax;            /*foreign byte, half a knot     */
		if (col_count == 0) {                /*the column opens right here   */
			if (place < 0)
				return pn_err_syntax;        /*knots past the free end       */
			col_kind = kind;
			col_place = place;
		} else if (kind != col_kind) {
			return pn_err_syntax;            /*two knots in one column never */
		}                                    /*came off the loom that way    */
		if (col_count == 9)
			return pn_err_range;             /*a tenth turn in this column   */
		col_count++;
		p += SIGN_BYTES;
	}
	if (col_count != 0) {                     /*the units column closes here   */
		if ((col_kind == 1) != (place == 0))
			return pn_err_syntax;
		sum += (uint64_t)col_count * pow10[place];
	}
	/*NO "walk ended at place -1" demand from muscle memory of a
	 *full-cord parser: a cord may END on a blank place (10 renders
	 *"• " — the units line stays empty), and with places = spaces + 1
	 *already enforced up front, the walk cannot land anywhere else.
	 *The only structural demand left is made by the columns them-
	 *selves: col_place must equal the place they close at. */

	*out = sum;
	return pn_ok;
}
