/* test_babylonian.c — positional cuneiform rendering.
 *
 * The signs are written as C11 universal character names so this file
 * stays pure ASCII:
 *
 *   DISH  U+12079  vertical wedge = 1    (UTF-8 F0 92 81 B9)
 *   U     U+1230B  corner wedge   = 10   (UTF-8 F0 92 8C 8B)
 *
 * Golden renderings from https://en.wikipedia.org/wiki/Babylonian_numerals:
 *   23   = uu...  2 corner wedges + 3 verticals
 *   8583 = 2*60^2 + 23*60 + 3 rendered place by place.
 */

#include "check.h"

#define DISH "\U00012079"
#define U    "\U0001230B"
#define TEN_DISH DISH DISH DISH DISH DISH DISH DISH DISH DISH DISH

/* Count whole sign occurrences in a rendered string. */
static int count_sigs(const char *buf, const char *sig)
{
	int n = 0;
	for (const char *p = buf; (p = strstr(p, sig)) != NULL; p += strlen(sig))
		n++;
	return n;
}

/* Render into a generous buffer, asserting render_size == strlen. */
static void render_ok(pn_sex60 v, char *buf, size_t buflen)
{
	size_t need = pn_babylonian_render_size(v);
	CHECK_ST(pn_babylonian_render(v, buf, need + 1), pn_ok);
	CHECK(strlen(buf) == need);
	(void)buflen;
}

static pn_sex60 mk(const uint8_t *places, uint8_t n, uint8_t radix)
{
	pn_sex60 v;
	memset(&v, 0, sizeof v);
	memcpy(v.places, places, n);
	v.n_places = n;
	v.radix = radix;
	return v;
}

int main(void)
{
	char buf[4096];

	/* --- single places: d tens-wedges then d%10 unit-wedges ---------- */
	static const uint8_t one[] = { 1 };
	static const uint8_t nine[] = { 9 };
	static const uint8_t ten[] = { 10 };
	static const uint8_t twentythree[] = { 23 };
	static const uint8_t fiftyseven[] = { 57 };
	static const uint8_t fiftynine[] = { 59 };

	render_ok(mk(one, 1, 1), buf, sizeof buf);
	CHECK(strcmp(buf, DISH) == 0);
	CHECK((unsigned char)buf[0] == 0xF0 && (unsigned char)buf[1] == 0x92 &&
			(unsigned char)buf[2] == 0x81 && (unsigned char)buf[3] == 0xB9);

	render_ok(mk(nine, 1, 1), buf, sizeof buf);
	CHECK(count_sigs(buf, DISH) == 9 && count_sigs(buf, U) == 0);

	render_ok(mk(ten, 1, 1), buf, sizeof buf);
	CHECK(strcmp(buf, U) == 0);

	/* Wikipedia's example: 23 = two U + three DISH. */
	render_ok(mk(twentythree, 1, 1), buf, sizeof buf);
	CHECK(strcmp(buf, U U DISH DISH DISH) == 0);

	/* 57 = five U + seven DISH. */
	render_ok(mk(fiftyseven, 1, 1), buf, sizeof buf);
	CHECK(count_sigs(buf, U) == 5 && count_sigs(buf, DISH) == 7);

	render_ok(mk(fiftynine, 1, 1), buf, sizeof buf);
	CHECK(count_sigs(buf, U) == 5 && count_sigs(buf, DISH) == 9);

	/* Every single place 1..59: wedge counts must be exact. */
	for (unsigned d = 1; d <= 59; d++) {
		uint8_t place = (uint8_t)d;
		render_ok(mk(&place, 1, 1), buf, sizeof buf);
		CHECK(count_sigs(buf, U) == (int)(d / 10));
		CHECK(count_sigs(buf, DISH) == (int)(d % 10));
		CHECK(strlen(buf) == (d / 10 + d % 10) * 4);
	}

	/* --- multi-place values ------------------------------------------- */

	/* The golden rendering: 8583 = 2,23,3.
	*  DISH DISH  space  UU + 3 DISH  space  3 DISH */
	static const uint8_t _8583[] = { 2, 23, 3 };
	render_ok(mk(_8583, 3, 3), buf, sizeof buf);
	CHECK(strcmp(buf,
			DISH DISH " "
			U U DISH DISH DISH " "
			DISH DISH DISH) == 0);

	/* 3601 = 1,0,1: the middle place is the historical gap — two
	*  spaces where no sign was impressed. */
	static const uint8_t _3601[] = { 1, 0, 1 };
	render_ok(mk(_3601, 3, 3), buf, sizeof buf);
	CHECK(strcmp(buf, DISH "    " DISH) == 0);

	/* The documented ambiguity, made testable: 60 written {1,0} and
	*  1 written {1,0} with a different radix — the tablet itself cannot
	*  tell them apart, so the renderer must not either. */
	static const uint8_t one_zero[] = { 1, 0 };
	render_ok(mk(one_zero, 2, 2), buf, sizeof buf);
	char buf2[4096];
	render_ok(mk(one_zero, 2, 1), buf2, sizeof buf2);
	CHECK(strcmp(buf, buf2) == 0);
	CHECK(strcmp(buf, DISH "   ") == 0);   /* wedge + joiner + 2-space gap */

	/* Negative sign (our anachronism) leads the rendering. */
	pn_sex60 negv = mk(one, 1, 1);
	negv.neg = true;
	render_ok(negv, buf, sizeof buf);
	CHECK(buf[0] == '-' && strcmp(buf + 1, DISH) == 0);

	/* Fractional places render like integer ones: no radix point was
	*  ever drawn.  {0,45} = 0;45 and {45} render alike. */
	static const uint8_t zero_45[] = { 0, 45 };
	static const uint8_t just45[] = { 45 };
	render_ok(mk(zero_45, 2, 1), buf, sizeof buf);
	render_ok(mk(just45, 1, 0), buf2, sizeof buf2);
	CHECK(strcmp(buf + 3, buf2) == 0);     /* past the "  " gap + joiner */

	/* --- errors -------------------------------------------------------- */

	/* NULL buffer. */
	CHECK_ST(pn_babylonian_render(mk(one, 1, 1), NULL, 16), pn_err_null);

	/* Buffer one byte short of need + NUL fails and leaves buf alone. */
	pn_sex60 v1 = mk(one, 1, 1);
	memset(buf, 'x', 8);
	CHECK_ST(pn_babylonian_render(v1, buf, 4), pn_err_buffer);
	CHECK(memcmp(buf, "xxxx", 4) == 0);

	/* Malformed values must not print wedges. */
	pn_sex60 bad = mk(one, 1, 1);
	bad.n_places = 0;
	memset(buf, 'x', 8);
	CHECK_ST(pn_babylonian_render(bad, buf, sizeof buf), pn_err_domain);
	CHECK(memcmp(buf, "xxxx", 4) == 0);

	bad = mk(one, 1, 1);
	bad.places[0] = 60;
	CHECK_ST(pn_babylonian_render(bad, buf, sizeof buf), pn_err_domain);

	bad = mk(one_zero, 2, 2);
	bad.radix = 3;                          /* radix > n_places */
	CHECK_ST(pn_babylonian_render(bad, buf, sizeof buf), pn_err_domain);

	/* render_size is total: a zero-place value still measures. */
	static const uint8_t zero[] = { 0 };
	CHECK(pn_babylonian_render_size(mk(zero, 1, 1)) == 2);

	/* --- parsing ---------------------------------------------------------
	*
	* Round trip first: whatever the renderer impressed, the parser must
	* read back — places, count and sign — whether handed the NUL-
	* terminated string or just its exact bytes. */
	static const uint8_t rt1[] = { 1 };
	static const uint8_t rt2[] = { 59 };
	static const uint8_t rt3[] = { 2, 23, 3 };
	static const uint8_t rt4[] = { 1, 0, 1 };
	static const uint8_t rt5[] = { 1, 0 };
	static const uint8_t rt6[] = { 0 };
	static const uint8_t rt7[] = { 0, 0 };
	static const uint8_t rt8[] = { 0, 45 };
	static const uint8_t rt9[] = { 3, 0, 0, 59 };
	static const uint8_t rt10[PN_SEX60_MAX_PLACES] = {
			59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59
	};
	static const uint8_t *const rts[] = {
			rt1, rt2, rt3, rt4, rt5, rt6, rt7, rt8, rt9, rt10
	};
	static const uint8_t rt_lens[] = { 1, 1, 3, 3, 2, 1, 2, 2, 4, 16 };

	for (size_t c = 0; c < sizeof rts / sizeof rts[0]; c++) {
		for (int neg = 0; neg <= 1; neg++) {
			pn_sex60 v = mk(rts[c], rt_lens[c], rt_lens[c]);
			v.neg = neg != 0;
			char rendered[4096];
			size_t need = pn_babylonian_render_size(v);
			CHECK_ST(pn_babylonian_render(v, rendered, need + 1), pn_ok);
			pn_sex60 w;
			CHECK_ST(pn_babylonian_parse(rendered, strlen(rendered), &w),
					pn_ok);
			CHECK(w.n_places == rt_lens[c] && w.neg == v.neg);
			CHECK(memcmp(w.places, rts[c], rt_lens[c]) == 0);
			CHECK(w.radix == rt_lens[c]);       /* period-style reading */
			/* the length-based call, NUL not included */
			CHECK_ST(pn_babylonian_parse(rendered, need, &w), pn_ok);
			CHECK(w.n_places == rt_lens[c]);
		}
	}

	/* The 8583 reading, spelled out. */
	static const uint8_t expect_8583[] = { 2, 23, 3 };
	pn_sex60 read;
	const char *tablet =
			DISH DISH " "
			U U DISH DISH DISH " "
			DISH DISH DISH;
	CHECK_ST(pn_babylonian_parse(tablet, strlen(tablet), &read), pn_ok);
	CHECK(read.n_places == 3 && read.radix == 3 && !read.neg);
	CHECK(memcmp(read.places, expect_8583, sizeof expect_8583) == 0);

	/* Errors: the strict spacing law and the canonical group shape.
	* (Each leaves *out untouched — checked via the sentinel below.) */
	static const char *const bad_strings[] = {
			"",                 /* nothing at all                        */
			" ",            	   /* one space is no tablet                */
			"    ",            /* four spaces: not 3k-1                  */
			"abc",               /* not signs, not spaces                  */
			"\t" DISH,           /* tabs are not the scribe's separator    */
			DISH U,                               /* DISH before U          */
			"\x22",              /* an embedded quote byte                 */
	};
	for (size_t i = 0; i < sizeof bad_strings / sizeof bad_strings[0]; i++) {
		read = mk(one, 1, 1);
		read.n_places = 7;                     /* sentinel */
		CHECK_ST(pn_babylonian_parse(bad_strings[i], strlen(bad_strings[i]),
				&read), pn_err_syntax);
		CHECK(read.n_places == 7);             /* untouched */
	}

	/* Six U signs overflow one place: that is range, not grammar. */
	read = mk(one, 1, 1);
	read.n_places = 7;
	CHECK_ST(pn_babylonian_parse(U U U U U U, sizeof(U U U U U U) - 1,
			&read), pn_err_range);
	CHECK(read.n_places == 7);

	/* Ten DISH signs overflow one place too: range, not grammar. */
	read = mk(one, 1, 1);
	read.n_places = 7;
	CHECK_ST(pn_babylonian_parse(TEN_DISH, sizeof(TEN_DISH) - 1, &read),
				pn_err_range);
	CHECK(read.n_places == 7);

	/* Seventeen places: too deep a tablet. */
	char deep[PN_SEX60_MAX_PLACES * 5 + 2];
	char *dp = deep;
	for (unsigned i = 0; i < PN_SEX60_MAX_PLACES + 1; i++) {
		memcpy(dp, DISH, 4);
		dp += 4;
		if (i < PN_SEX60_MAX_PLACES)
			*dp++ = ' ';
		}
	*dp = '\0';
	CHECK_ST(pn_babylonian_parse(deep, (size_t)(dp - deep), &read),
			pn_err_range);

	/* Nulls. */
	CHECK_ST(pn_babylonian_parse(NULL, 4, &read), pn_err_null);
	CHECK_ST(pn_babylonian_parse(DISH, 4, NULL), pn_err_null);

	/* A cut-off sign (3 of DISH's 4 bytes) is not a sign. */
	CHECK_ST(pn_babylonian_parse(DISH, 3, &read), pn_err_syntax);

	/* A negative round-trips its sign. */
	pn_sex60 neg_zero = mk(rt6, 1, 1);
	neg_zero.neg = true;
	char neg_rendered[16];
	size_t neg_need = pn_babylonian_render_size(neg_zero);
	CHECK_ST(pn_babylonian_render(neg_zero, neg_rendered, neg_need + 1), pn_ok);
	CHECK_ST(pn_babylonian_parse(neg_rendered, neg_need, &read), pn_ok);
	CHECK(read.neg && read.n_places == 1 && read.places[0] == 0);

	return CHECK_RUN();
}
