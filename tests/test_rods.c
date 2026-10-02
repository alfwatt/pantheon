/* test_rods.c — counting rod numeral rendering.
   	  	 	*
   	  	 	* The rods are written as C11 universal character names so this
   	  	 	* file stays pure ASCII:
   	  	 	*
   	  	 	*   U1D360..U1D368  COUNTING ROD UNIT DIGIT ONE..NINE  𝍠–𝍨
   	  	 	*   U1D369..U1D371  COUNTING ROD TENS DIGIT ONE..NINE  𝍩–𝍱
   	  	 	*   all are 4 bytes of UTF-8 with stem F0 9D 8D
   	  	 	*
   	  	 	* Positions alternate form: units and every even place take the
   	  	 	* unit set, odd places the tens set.  Zero is the blank column —
   	  	 	* a space — and a well-formed string holds exactly (places − 1)
   	* spaces: 23 renders as tens-2 and unit-3 with one space
   	* between them, 101 with the tens column blank (two spaces),
   	* and 100 with two trailing spaces.
   	* See https://en.wikipedia.org/wiki/Counting_rods .
   	  	 	*/

#include "check.h"

#define U1   "\U0001D360"   /* unit 1  𝍠  UTF-8: F0 9D 8D A0 */
#define U2   "\U0001D361"
#define U3   "\U0001D362"
#define U9   "\U0001D368"
#define T1   "\U0001D369"   /* tens 1  𝍩 */
#define T2   "\U0001D36A"   /* tens 2  𝍪 */
#define T9   "\U0001D371"
#define QUEN "\U00003007"    /* 〇: the modern transcription zero — foreign here */

/* Render into a generous buffer, asserting render_size == strlen. */
static void render_ok(uint64_t n, char *buf, size_t buflen)
{
	size_t need = pn_rods_render_size(n);
	CHECK_ST(pn_rods_render(n, buf, need + 1), pn_ok);
	CHECK(strlen(buf) == need);
	(void)buflen;
}

/* render(n) into buf and parse it back, checking the round trip. */
static void roundtrip(uint64_t n)
{
	char buf[256];
	size_t need = pn_rods_render_size(n);
	CHECK_ST(pn_rods_render(n, buf, need + 1), pn_ok);
	CHECK(strlen(buf) == need);
	uint64_t back = 0;
	CHECK_ST(pn_rods_parse(buf, strlen(buf), &back), pn_ok);
	CHECK(back == n);
}

static pn_status parse_str(const char *text, uint64_t *out)
{
	return pn_rods_parse(text, strlen(text), out);
}

int main(void)
{
	char buf[4096];
	uint64_t v;

				/* --- the lone rod and its raw bytes ------------------------------ */

	render_ok(1u, buf, sizeof buf);
	CHECK(strcmp(buf, U1) == 0);
				/* the one's raw bytes pinned, as the stroke's are */
	CHECK((unsigned char)buf[0] == 0xF0 && (unsigned char)buf[1] == 0x9D &&
				(unsigned char)buf[2] == 0x8D && (unsigned char)buf[3] == 0xA0);

				/* --- golden renderings ------------------------------------------- */

	render_ok(0u, buf, sizeof buf);                    /* the empty board */
	CHECK(buf[0] == '\0');
	render_ok(10u, buf, sizeof buf);                   /* blank units column */
	CHECK(strcmp(buf, T1 " ") == 0);
	render_ok(11u, buf, sizeof buf);
	CHECK(strcmp(buf, T1 " " U1) == 0);
	render_ok(20u, buf, sizeof buf);
	CHECK(strcmp(buf, T2 " ") == 0);
	render_ok(23u, buf, sizeof buf);
	CHECK(strcmp(buf, T2 " " U3) == 0);
	render_ok(100u, buf, sizeof buf);                  /* two trailing blanks */
	CHECK(strcmp(buf, U1 "  ") == 0);
	render_ok(1000u, buf, sizeof buf);
	CHECK(strcmp(buf, T1 "   ") == 0);
	render_ok(101u, buf, sizeof buf);                  /* blank tens column */
	CHECK(strcmp(buf, U1 "  " U1) == 0);
	render_ok(10001u, buf, sizeof buf);                /* three blanks + joiner */
	CHECK(strcmp(buf, U1 "    " U1) == 0);
	render_ok(1000000000000000000ull, buf, sizeof buf); /* place 18: unit set */
	CHECK(strcmp(buf, U1 "                  ") == 0);
	CHECK(pn_rods_render_size(0u) == 0u);
	CHECK(pn_rods_render_size(23u) == 9u);             /* 2 rods + 1 boundary  */
	CHECK(pn_rods_render_size(101u) == 10u);           /* 2 rods + 2 boundaries */
	CHECK(pn_rods_render_size(100u) == 6u);            /* 1 rod + 2 boundaries  */

				/* the maximum: nineteen 9-rods, alternating sets ------------------ */

	{
		char want[96];
		size_t w = 0;
		for (int i = 18; i >= 0; i--) {
			if (i != 18)
				want[w++] = ' ';
			const char *g = (i & 1) == 0 ? U9 : T9;
			memcpy(want + w, g, 4);
			w += 4;
		}
		want[w] = '\0';
		render_ok(PN_ROD_MAX_VALUE, buf, sizeof buf);
		CHECK(strcmp(buf, want) == 0);
		CHECK(pn_rods_render_size(PN_ROD_MAX_VALUE) == 4u * 19u + 18u);
	}

				/* --- render error cases ------------------------------------------ */

	CHECK_ST(pn_rods_render(1u, NULL, 10), pn_err_null);
	render_ok(23u, buf, sizeof buf);
	CHECK_ST(pn_rods_render(23u, buf, pn_rods_render_size(23u)),
			pn_err_buffer);                          /* the exact-minus-NUL cut */
	CHECK_ST(pn_rods_render(10000000000000000000ull, buf, sizeof buf),
			pn_err_range);                            /* 10^19: a twentieth column */
	CHECK_ST(pn_rods_render(UINT64_MAX, buf, sizeof buf), pn_err_range);

				/* --- parse error cases ------------------------------------------- */

	CHECK_ST(pn_rods_parse(NULL, 0, &v), pn_err_null);
	CHECK_ST(pn_rods_parse(buf, 3, NULL), pn_err_null);

	uint64_t probe;
	CHECK_ST(parse_str("", &probe), pn_ok);            /* the empty board is 0 */
	CHECK(probe == 0u);
	CHECK_ST(parse_str(" " U1, &probe), pn_err_syntax);         /* leading space    */
	CHECK_ST(parse_str("   ", &probe), pn_err_syntax);          /* all spaces        */
	CHECK_ST(parse_str(T1 U1, &probe), pn_err_syntax);          /* no joiner         */
	CHECK_ST(parse_str(U1 " " U1, &probe), pn_err_syntax);      /* 11 in the wrong
	 	 	 	 	 	 	 	 	 	 	 	 	 		* columns: the tens rod must
	 	 	 	 	 	 	 	 	 	 	 	 	 		* come from the tens set       */
	CHECK_ST(parse_str(T1 " " T1, &probe), pn_err_syntax);      /* units rod in the
	 	 	 	 	 	 	 	 	 	 	 	 	 		* tens set                     */
	CHECK_ST(parse_str("\xF0\x9D\x8D", &probe), pn_err_syntax); /* rod cut off short */
	CHECK_ST(parse_str("\xF0\x9D\x8D\xB2", &probe),
			pn_err_syntax);                            /* the tally mark is no rod */
	CHECK_ST(parse_str("\xF0\x92\x81\xB9", &probe),
			pn_err_syntax);                            /* cuneiform DISH is no rod */
	CHECK_ST(parse_str(QUEN, &probe), pn_err_syntax);           /* 〇: not our zero  */
	CHECK_ST(parse_str("1", &probe), pn_err_syntax);            /* ASCII              */

				/* a well-spaced twentieth place: beyond the board, not bad grammar */

	{
		char long_str[128];                          /* 20 glyphs, 19 joiners    */
		size_t w = 0;
		for (int i = 19; i >= 0; i--) {
			if (i != 19)
				long_str[w++] = ' ';
			const char *g = (i & 1) == 0 ? U1 : T1; /* 1s in the right sets */
			memcpy(long_str + w, g, 4);
			w += 4;
		}
		long_str[w] = '\0';
		CHECK_ST(pn_rods_parse(long_str, w, &probe), pn_err_range);
	}

				/* --- round-trip sweep -------------------------------------------- */

	for (uint64_t n = 0; n < 10000; n++)
		roundtrip(n);                                /* two places of every shape */
	for (uint64_t p = 1; p <= 1000000000000000000ull; p *= 10)
		roundtrip(p);                                /* one rod at each height    */
	roundtrip(1010101u);                             /* blank after blank          */
	roundtrip(9090909u);
	roundtrip(10001u);
	roundtrip(1000000000000000000ull);
	roundtrip(PN_ROD_MAX_VALUE);
	roundtrip(1844674407370955161ull);               /* 19 digits, no blanks       */
	roundtrip(1000000000000000001ull);

	return CHECK_RUN();
}
