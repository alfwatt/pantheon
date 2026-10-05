/*  test_quipu.c — Inca positional decimal knot notation, transcribed.
   Signs as C11 universal character names (the source stays pure ASCII):
   SIMPLE   U+2022 bullet      one simple knot        (Locke's s)
   FIGURE   U+221E infinity    the figure-eight at the units line
   (Locke's E); repeated — d of them —
   the long knot of d turns   (d L)
   Golden cords from the https://en.wikipedia.org/wiki/Quipu worked
   examples: 731 = 7s 3s E, 804 = 8s X 4L, 1493 = 1s 4s 9s 3L — simple
   knots above the units line, the figure-eight and the long knot's
   turns at it, blank places as bare boundary spacing.  One law rules
   every expected string in this file — spaces = places - 1, exactly
   one boundary space per place step, blank places included — and the
   generator .hermes/tmp/gen_test_quipu.py spelled every cord, pad and
   error string from that law.  No space count here was hand-typed.
*/

#include "check.h"

#define SIMPLE "\u2022"
#define FIGURE "\u221e"

/*  d identical knots per digit 0..9.  A column is a RUN: no separator
   sits between the knots inside one column - the space is a place
   boundary and nothing else - so the rows concatenate to the empty
   string for digit 0, the blank place.
*/
static const char *const figure_col[10] = {
	"",
	FIGURE,
	FIGURE FIGURE,
	FIGURE FIGURE FIGURE,
	FIGURE FIGURE FIGURE FIGURE,
	FIGURE FIGURE FIGURE FIGURE FIGURE,
	FIGURE FIGURE FIGURE FIGURE FIGURE FIGURE,
	FIGURE FIGURE FIGURE FIGURE FIGURE FIGURE FIGURE,
	FIGURE FIGURE FIGURE FIGURE FIGURE FIGURE FIGURE FIGURE,
	FIGURE FIGURE FIGURE FIGURE FIGURE FIGURE FIGURE FIGURE FIGURE
};
static const char *const simple_col[10] = {
	"",
	SIMPLE,
	SIMPLE SIMPLE,
	SIMPLE SIMPLE SIMPLE,
	SIMPLE SIMPLE SIMPLE SIMPLE,
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE,
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE,
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE,
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE,
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE
};

/*  Padded columns of d*10, d*100, d*1000 per digit: k knot-macro
   literals plus (places - 1) boundary literals - 1, 2, 3 of them;
   every grown from truth()'s knot law at generation time (an array
   element juxtaposed with a literal would be illegal C).
*/
static const char *const tens_col[10] = {
	" ",
	SIMPLE " ",
	SIMPLE SIMPLE " ",
	SIMPLE SIMPLE SIMPLE " ",
	SIMPLE SIMPLE SIMPLE SIMPLE " ",
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE " ",
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE " ",
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE " ",
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE " ",
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE " "
};
static const char *const hundreds_col[10] = {
	" " " ",
	SIMPLE " " " ",
	SIMPLE SIMPLE " " " ",
	SIMPLE SIMPLE SIMPLE " " " ",
	SIMPLE SIMPLE SIMPLE SIMPLE " " " ",
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE " " " ",
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE " " " ",
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE " " " ",
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE " " " ",
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE " " " "
};
static const char *const thousands_col[10] = {
	" " " " " ",
	SIMPLE " " " " " ",
	SIMPLE SIMPLE " " " " " ",
	SIMPLE SIMPLE SIMPLE " " " " " ",
	SIMPLE SIMPLE SIMPLE SIMPLE " " " " " ",
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE " " " " " ",
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE " " " " " ",
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE " " " " " ",
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE " " " " " ",
	SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE " " " " " "
};

/*  Independent hand-spelling of the canonical cord, written without
   touching the library: peel the decimal places (least significant
   first); d whole knots per column - the figure-eight family at the
   units line, simple knots above it; one space per boundary; 0 is the
   cord with no knot.
*/
static void expect_render(uint64_t n, char *out)
{
	uint8_t dig[PN_QUIPU_MAX_PLACES + 1];
	int m = 0;
	while (n != 0) {
		dig[m++] = (uint8_t)(n % 10u);
		n /= 10u;
	}
	char *p = out;
	for (int i = m - 1; i >= 0; i--) {                /* most significant first */
		if (i != m - 1)
			*p++ = ' ';                                 /* one boundary per step */
		const char *g = (i == 0) ? figure_col[dig[i]] : simple_col[dig[i]];
		while (*g != '\0')
			*p++ = *g++;
	}
	*p = '\0';
}

/* Render into a buffer sized to the exact answer plus the NUL. */
static void render_ok(uint64_t n, char *buf, size_t buflen)
{
	size_t need = pn_quipu_render_size(n);
	CHECK_OK(pn_quipu_render(n, buf, need + 1, &err_), err_);
	CHECK(strlen(buf) == need);
	(void)buflen;
}

static bool parse_str(const char *text, uint64_t *out, pn_error **error)
{
	return pn_quipu_parse(text, strlen(text), out, error);
}

static void expect_syntax(const char *text)
{
	uint64_t back = 256u;                             /* sentinel */
	CHECK_ERROR(pn_quipu_parse(text, strlen(text), &back, &err_), err_, PN_ERROR_SYNTAX);
	CHECK(back == 256u);                              /* *out untouched */
}

static void expect_range(const char *text)
{
	uint64_t back = 256u;
	CHECK_ERROR(pn_quipu_parse(text, strlen(text), &back, &err_), err_, PN_ERROR_RANGE);
	CHECK(back == 256u);
}

int main(void)
{
	char buf[1024];
	char want[1024];
	uint64_t v;

		/* a knot and its raw bytes */
		/* render 1: Locke's E, the figure-eight knot */
	render_ok(1u, buf, sizeof buf);
	CHECK(strcmp(buf, FIGURE) == 0);

		/* the figure-eight's raw bytes pinned, as the stroke's are elsewhere */
	CHECK((unsigned char)buf[0] == 0xE2 && (unsigned char)buf[1] == 0x88 &&
			(unsigned char)buf[2] == 0x9E);

		/* render 4: Locke's 4L, a long knot of four turns */
	render_ok(4u, buf, sizeof buf);
	CHECK(strcmp(buf, FIGURE FIGURE FIGURE FIGURE) == 0);

		/* render 40: four simple knots over the blank units line */
	render_ok(40u, buf, sizeof buf);
	CHECK(strcmp(buf, SIMPLE SIMPLE SIMPLE SIMPLE " ") == 0);

		/* golden cords: the article's worked examples */

		/* golden 731 = Locke's 7s, 3s, E: three columns */
	render_ok(731u, buf, sizeof buf);
	CHECK(strcmp(buf, SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE " " SIMPLE SIMPLE SIMPLE " " FIGURE) == 0);

		/* golden 804 = Locke's 8s, X, 4L: the blank tens place keeps */
		/* its boundary — the cord runs on between TWO spaces */
	render_ok(804u, buf, sizeof buf);
	CHECK(strcmp(buf, SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE " " " " FIGURE FIGURE FIGURE FIGURE) == 0);

		/* golden 1493 = Locke's 1s, 4s, 9s, 3L: four columns */
	render_ok(1493u, buf, sizeof buf);
	CHECK(strcmp(buf, SIMPLE " " SIMPLE SIMPLE SIMPLE SIMPLE " " SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE " " FIGURE FIGURE FIGURE) == 0);

		/* the boundary nobody may tidy: 10 is one simple knot over the blank */
		/* units line — its closing space IS the cord */
	render_ok(10u, buf, sizeof buf);
	CHECK(strcmp(buf, SIMPLE " ") == 0);
	CHECK(pn_quipu_render_size(10u) == 4u);

		/* 4000: four knots plus THREE boundaries — every place step keeps its */
		/* space, the two blank places included */
	render_ok(4000u, buf, sizeof buf);
	CHECK(strcmp(buf, SIMPLE SIMPLE SIMPLE SIMPLE " " " " " ") == 0);

		/* 100: one simple knot over two blank places */
	render_ok(100u, buf, sizeof buf);
	CHECK(strcmp(buf, SIMPLE " " " ") == 0);

		/*  every digit at every height
	   The pad literals carry the boundary counts the law derives — tens
	   1, hundreds 2, thousands 3 (machine-derived at generation time) —
	   and every row is cross-checked against expect_render, so a stray
	   space in the typed pad or in the hand-speller lights up the other.
	*/

	for (unsigned d = 1u; d < 10u; d++) {
		render_ok(d, buf, sizeof buf);
		CHECK(strcmp(buf, figure_col[d]) == 0);
				(void)expect_render(d * 10u, want);
		render_ok(d * 10u, buf, sizeof buf);
		CHECK(strcmp(buf, want) == 0);         /* hand-spelled tens cord */
		CHECK(strcmp(buf, tens_col[d]) == 0);   /* literal-juxtaposed twin */
				(void)expect_render(d * 100u, want);
		render_ok(d * 100u, buf, sizeof buf);
		CHECK(strcmp(buf, want) == 0);
		CHECK(strcmp(buf, hundreds_col[d]) == 0);
				(void)expect_render(d * 1000u, want);
		render_ok(d * 1000u, buf, sizeof buf);
		CHECK(strcmp(buf, want) == 0);
		CHECK(strcmp(buf, thousands_col[d]) == 0);
		if (pn_fails)
			break;
	}

		/*  zero: the absence of a knot
	   No column, no boundary: the law speaks of cords, and the cord no
	   knot was IS the zero the Inca never wrote a sign for.
	*/

	CHECK(pn_quipu_render_size(0u) == 0u);
	memset(buf, 'x', 4);
	CHECK_OK(pn_quipu_render(0u, buf, 1, &err_), err_);
	CHECK(buf[0] == '\0');
	v = UINT64_MAX;
	CHECK_OK(parse_str("", &v, &err_), err_);
	CHECK(v == 0u);

		/* render_size exact everywhere, swept */

	for (uint64_t n = 0; n < 160000u; n++) {
		(void)expect_render(n, want);
		CHECK(pn_quipu_render_size(n) == strlen(want));
		if (pn_fails)
			break;
	}

		/* errors: render */

	CHECK_ERROR(pn_quipu_render(1u, pn_nothing(), 16, &err_), err_, PN_ERROR_NULL);

		/* one past nineteen places: 10^19 overflows the uint64_t shelf */
	memset(buf, 'x', 8);
	CHECK_ERROR(pn_quipu_render(PN_QUIPU_MAX_VALUE + 1ull, buf,
			sizeof buf, &err_), err_, PN_ERROR_RANGE);
	CHECK(memcmp(buf, "xxxx", 4) == 0);
	CHECK_ERROR(pn_quipu_render(UINT64_MAX, buf, sizeof buf, &err_), err_, PN_ERROR_RANGE);

		/* a buffer short of need + NUL fails and leaves buf alone */
	memset(buf, 'x', 8);
	CHECK_ERROR(pn_quipu_render(1u, buf, 3, &err_), err_, PN_ERROR_BUFFER);   /* need is 3 */
	CHECK(memcmp(buf, "xxxx", 4) == 0);

		/*  parsing: the round trip, exhaustive
	   Whatever the loom tied, the cord reads back — through both the
	   NUL-terminated spelling and the exact knot count.
	*/

	for (uint64_t n = 0; n < 160000u; n++) {
		char cord[1024];
		size_t need = pn_quipu_render_size(n);
		CHECK_OK(pn_quipu_render(n, cord, need + 1, &err_), err_);
		(void)expect_render(n, want);
		CHECK(strcmp(cord, want) == 0);
		uint64_t back = UINT64_MAX;
		CHECK_OK(pn_quipu_parse(cord, strlen(cord), &back, &err_), err_);
		CHECK(back == n);
		back = UINT64_MAX;
		CHECK_OK(pn_quipu_parse(cord, need, &back, &err_), err_); /* exact bytes */
		CHECK(back == n);
		if (pn_fails)
			break;                             /* the first failure names n */
	}

		/* deep places one row at a time: d x 10^k, every digit, k = 4..18 */
	static const uint64_t deep[] = {
		10000ull, 100000ull, 1000000ull, 10000000ull, 100000000ull,
		1000000000ull, 10000000000ull, 100000000000ull,
		1000000000000ull, 10000000000000ull, 100000000000000ull,
		1000000000000000ull, 10000000000000000ull,
		100000000000000000ull, 1000000000000000000ull      /* 10^18 */
	};
	for (size_t i = 0; i < sizeof deep / sizeof deep[0]; i++) {
		for (unsigned d = 1u; d < 10u; d++) {
			uint64_t n = (uint64_t)d * deep[i];
			char cord[1024];
			size_t need = pn_quipu_render_size(n);
			CHECK_OK(pn_quipu_render(n, cord, need + 1, &err_), err_);
			uint64_t back = UINT64_MAX;
			CHECK_OK(pn_quipu_parse(cord, strlen(cord), &back, &err_), err_);
			CHECK(back == n);
			back = UINT64_MAX;
			CHECK_OK(pn_quipu_parse(cord, need, &back, &err_), err_);
			CHECK(back == n);
		}
	}

		/* the very top of the loom: nineteen columns, all nines */
	char top[1024];
	(void)expect_render(PN_QUIPU_MAX_VALUE, want);
	size_t top_need = pn_quipu_render_size(PN_QUIPU_MAX_VALUE);
	CHECK(top_need == strlen(want));
	CHECK_OK(pn_quipu_render(PN_QUIPU_MAX_VALUE, top,
			sizeof top, &err_), err_);
	uint64_t back = UINT64_MAX;
	CHECK_OK(parse_str(top, &back, &err_), err_);
	CHECK(back == PN_QUIPU_MAX_VALUE);

		/*  errors: the strict canonical cord
	   Every string below was spelled from the law by the generator; each
	   check leaves *out untouched — watched via the sentinel.
	*/

		/* a lone simple knot: bullets never live at the units line */
	expect_syntax(SIMPLE);
		/* a whole column of simple knots at the units line */
	expect_syntax(SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE);
		/* the figure-eight family hung over a closing boundary */
	expect_syntax(FIGURE " ");
		/* one column, two knot types jammed with no seam */
	expect_syntax(SIMPLE FIGURE);
		/* a mixed column of simple knots and turns */
	expect_syntax(SIMPLE SIMPLE FIGURE);
		/* the cord never starts in a gutter */
	expect_syntax(" " SIMPLE " " FIGURE);
		/* nor ends in a gutter */
	expect_syntax(SIMPLE " " FIGURE " ");
		/* a single space is not the blank cord */
	expect_syntax(" ");
		/* four spaces are no cord without knots */
	expect_syntax(" " " " " " " " " ");
		/* not knots at all */
	expect_syntax("abc");
		/* the chronicler's minus */
	expect_syntax("-1");
		/* tabs are not how cords hang */
	expect_syntax("\t" FIGURE);
		/* an ASCII byte between knots */
	expect_syntax(SIMPLE "A" SIMPLE);
		/* U+2023 TOP BULLET is no knot */
	expect_syntax("\xE2\x80\xA3");
		/* U+221D PROPORTIONAL TO is the wrong knot */
	expect_syntax("\xE2\x88\x9D");
		/* U+00B7 MIDDLE DOT is no knot */
	expect_syntax("\xC2\xB7");
		/* a Mayan dot is no knot */
	expect_syntax("\xF0\x9D\x8B\xA1");
		/* a Brahmi digit is no knot */
	expect_syntax("\xF0\x91\x81\x92");
		/* a counting rod is no knot */
	expect_syntax("\xF0\x9D\x8D\xA0");
		/* a knot cut off after two of its three bytes */
	expect_syntax("\xE2\x80");
		/* an infinity cut off after its first byte */
	expect_syntax("\xE2");

		/*  ten of a kind is more turns than any digit holds (babylonian's
	   tenth-DISH ruling) and a twentieth column is more cord than a
	   uint64_t holds: both land in PN_ERROR_RANGE
	*/
		/* ten simple knots over the blank units: more turns than any digit holds */
	expect_range(SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE SIMPLE " ");
		/* ten turns at the units line */
	expect_range(FIGURE FIGURE FIGURE FIGURE FIGURE FIGURE FIGURE FIGURE FIGURE FIGURE);
		/* a twentieth column past the page: 19 places fit, 20 do not */
	expect_range(SIMPLE " " SIMPLE " " SIMPLE " " SIMPLE " " SIMPLE " " SIMPLE " " SIMPLE " " SIMPLE " " SIMPLE " " SIMPLE " " SIMPLE " " SIMPLE " " SIMPLE " " SIMPLE " " SIMPLE " " SIMPLE " " SIMPLE " " SIMPLE " " SIMPLE " " SIMPLE);

		/* Nulls. */
	CHECK_ERROR(pn_quipu_parse(pn_nothing(), 4, &back, &err_), err_, PN_ERROR_NULL);
	CHECK_ERROR(pn_quipu_parse(FIGURE, 3, pn_nothing(), &err_), err_, PN_ERROR_NULL);

		/*  A cut-off knot: two of the bullet's three bytes stop mid-UTF-8 —
	   two whole knots would be six bytes; seven is half a knot.
	*/
	back = 256u;
	CHECK_ERROR(pn_quipu_parse(SIMPLE SIMPLE SIMPLE, 2u * 3u + 1u,
			&back, &err_), err_, PN_ERROR_SYNTAX);
	CHECK(back == 256u);

		/*  Half a column: a bullet, a boundary, then the infinity cut short
	   - its two leading bytes plus a foreign tail byte.
	*/
	back = 256u;
	CHECK_ERROR(pn_quipu_parse(SIMPLE " \xE2\x88\x41", 3u + 1u + 3u,
			&back, &err_), err_, PN_ERROR_SYNTAX);
	CHECK(back == 256u);

	return CHECK_RUN();
}
