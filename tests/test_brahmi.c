/* test_brahmi.c — Brahmi numeral rendering and parsing.
	*
	* The signs are written as C11 universal character names so this file
	* stays pure ASCII (signs at least):
	*
	*   ONE..NINE    U+11052..U+1105A  dedicated signs   = 1..9
	*   TEN..NINETY  U+1105B..U+11063  dedicated signs   = 10..90
	*   HUNDRED      U+11064                              = 100
	*   THOUSAND     U+11065                              = 1 000
	*   JOINER       U+1107F          ligature join       (Mn)
	*
	* Golden renderings from https://en.wikipedia.org/wiki/Brahmi_numerals:
	* the Sasaram edict's 256 (200 ligatured + fifty + six) and the
	* Damasena coin's 153 (100 + fifty + three, all additive).
	*/

#include "check.h"

#define ONE       "\U00011052"
#define TWO       "\U00011053"
#define THREE     "\U00011054"
#define FOUR      "\U00011055"
#define FIVE      "\U00011056"
#define SIX       "\U00011057"
#define SEVEN     "\U00011058"
#define EIGHT     "\U00011059"
#define NINE      "\U0001105A"
#define TEN       "\U0001105B"
#define TWENTY    "\U0001105C"
#define THIRTY    "\U0001105D"
#define FORTY     "\U0001105E"
#define FIFTY     "\U0001105F"
#define SIXTY     "\U00011060"
#define SEVENTY   "\U00011061"
#define EIGHTY    "\U00011062"
#define NINETY    "\U00011063"
#define HUNDRED   "\U00011064"
#define THOUSAND  "\U00011065"
#define JOINER    "\U0001107F"

/* The twenty number signs, in code-point order, with their values. */
static const char *const unit_signs[9] = {
	ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE
};
static const char *const ten_signs[9] = {
	TEN, TWENTY, THIRTY, FORTY, FIFTY, SIXTY, SEVENTY, EIGHTY, NINETY
};

/* The canonical Brahmi spelling of any n <= 9999, spelled out here
	independently of the library: power terms ligatured, additive
	terms bare. */
static void expect_render(uint64_t n, char *out)
{
	char *p = out;
	unsigned d;

	d = (unsigned)((n / 1000u) % 10u);
	if (d == 1u) {
		memcpy(p, THOUSAND, 4); p += 4;
	} else if (d >= 2u) {
		memcpy(p, THOUSAND, 4); p += 4;
		memcpy(p, JOINER, 4);   p += 4;
		memcpy(p, unit_signs[d - 1u], 4); p += 4;
	}
	d = (unsigned)((n / 100u) % 10u);
	if (d == 1u) {
		memcpy(p, HUNDRED, 4); p += 4;
	} else if (d >= 2u) {
		memcpy(p, HUNDRED, 4);  p += 4;
		memcpy(p, JOINER, 4);   p += 4;
		memcpy(p, unit_signs[d - 1u], 4); p += 4;
	}
	d = (unsigned)((n / 10u) % 10u);
	if (d != 0u) {
		memcpy(p, ten_signs[d - 1u], 4); p += 4;
	}
	d = (unsigned)(n % 10u);
	if (d != 0u) {
		memcpy(p, unit_signs[d - 1u], 4); p += 4;
	}
	*p = '\0';
}

/* Render into a generous buffer, asserting render_size == strlen. */
static void render_ok(uint64_t n, char *buf, size_t buflen)
{
	size_t need = pn_brahmi_render_size(n);
	CHECK_OK(pn_brahmi_render(n, buf, need + 1, &err_), err_);
	CHECK(strlen(buf) == need);
	(void)buflen;
}

int main(void)
{
	char buf[4096];
	char want[4096];

	/* --- the twenty-two single-term values ---------------------------- */

	/* a bare power sign means 1 x power: the joiner never shows up */
	render_ok(1u, buf, sizeof buf);
	CHECK(strcmp(buf, ONE) == 0);
	/* the ONE sign's raw bytes pinned, as the stroke's are */
	CHECK((unsigned char)buf[0] == 0xF0 && (unsigned char)buf[1] == 0x91 &&
			(unsigned char)buf[2] == 0x81 && (unsigned char)buf[3] == 0x92);
	for (unsigned d = 1; d <= 9; d++) {
		render_ok(d, buf, sizeof buf);
		CHECK(strcmp(buf, unit_signs[d - 1u]) == 0);
		render_ok(d * 10u, buf, sizeof buf);
		CHECK(strcmp(buf, ten_signs[d - 1u]) == 0);
	}
	render_ok(100u, buf, sizeof buf);
	CHECK(strcmp(buf, HUNDRED) == 0);
	render_ok(1000u, buf, sizeof buf);
	CHECK(strcmp(buf, THOUSAND) == 0);

	/* --- the ligatured powers: 2..9 x 100 and x 1000 ------------------ */

	for (unsigned d = 2; d <= 9; d++) {
				/* 400 is HUNDRED JOINER FOUR — power first, per N4166 */
		render_ok(d * 100u, buf, sizeof buf);
		(void)expect_render(d * 100u, want);
		CHECK(strcmp(buf, want) == 0);
		CHECK(strlen(buf) == 12u);

		render_ok(d * 1000u, buf, sizeof buf);
		(void)expect_render(d * 1000u, want);
		CHECK(strcmp(buf, want) == 0);
		CHECK(strlen(buf) == 12u);
	}

	/* the 400 spelling from the N4166 joiner proposal itself:
	 * 100 JOINER 4 ligatured versus 100 4 additive */
	render_ok(400u, buf, sizeof buf);
	CHECK(strcmp(buf, HUNDRED JOINER FOUR) == 0);
	render_ok(104u, buf, sizeof buf);
	CHECK(strcmp(buf, HUNDRED FOUR) == 0);
	/* and the two readings are different strings, as they must be */
	CHECK(strcmp(buf, HUNDRED JOINER FOUR) != 0);

	/* --- golden historical renderings ---------------------------------- */

	/* Ashoka's Minor Rock Edict No.1 at Sasaram: 256 nights on tour —
	 * 200 as one ligature, then fifty, then six. */
	render_ok(256u, buf, sizeof buf);
	CHECK(strcmp(buf, HUNDRED JOINER TWO FIFTY SIX) == 0);

	/* The coin of the Western Satrap Damasena, Saka era 153:
	 * 100 + 50 + 3, every term additive, no joiner in sight. */
	render_ok(153u, buf, sizeof buf);
	CHECK(strcmp(buf, HUNDRED FIFTY THREE) == 0);

	/* 47: two signs, not eleven — the point of the whole system. */
	render_ok(47u, buf, sizeof buf);
	CHECK(strcmp(buf, FORTY SEVEN) == 0);
	CHECK(strlen(buf) == 8u);

	/* 2 222: the 2 sign three times over — slots, not sign identity,
	 * rule the parser. */
	render_ok(2222u, buf, sizeof buf);
	CHECK(strcmp(buf,
			THOUSAND JOINER TWO
			HUNDRED JOINER TWO
			TWENTY TWO) == 0);

	/* the scribe's full set: every slot filled, 9 999 */
	render_ok(9999u, buf, sizeof buf);
	CHECK(strcmp(buf,
			THOUSAND JOINER NINE
			HUNDRED JOINER NINE
			NINETY NINE) == 0);
	CHECK(strlen(buf) == 32u);

	/* Gaps are silent, not drawn: 1 001 is thousand + one, nothing in
	 * between — no zero sign exists to fill the blank. */
	render_ok(1001u, buf, sizeof buf);
	CHECK(strcmp(buf, THOUSAND ONE) == 0);
	render_ok(9009u, buf, sizeof buf);
	CHECK(strcmp(buf, THOUSAND JOINER NINE NINE) == 0);

	/* --- zero: the blank ------------------------------------------------ */

	CHECK(pn_brahmi_render_size(0u) == 0);
	memset(buf, 'x', 4);
	CHECK_OK(pn_brahmi_render(0u, buf, 1, &err_), err_);
	CHECK(buf[0] == '\0');

	/* --- render_size is exact everywhere -------------------------------- */

	for (uint64_t n = 0; n <= PN_BRAHMI_MAX_VALUE; n++) {
			(void)expect_render(n, want);
		CHECK(pn_brahmi_render_size(n) == strlen(want));
		if (pn_fails)
			break;
	}

	/* --- errors ---------------------------------------------------------- */

	CHECK_ERROR(pn_brahmi_render(1u, pn_nothing(), 16, &err_), err_, PN_ERROR_NULL);

	/* One past the four filled slots is range, not silence: there is
	 * no sign above the thousand. */
	memset(buf, 'x', 8);
	CHECK_ERROR(pn_brahmi_render(PN_BRAHMI_MAX_VALUE + 1u, buf,
			sizeof buf, &err_), err_, PN_ERROR_RANGE);
	CHECK(memcmp(buf, "xxxx", 4) == 0);
	CHECK_ERROR(pn_brahmi_render(UINT64_MAX, buf, sizeof buf, &err_), err_, PN_ERROR_RANGE);

	/* Buffer one byte short of need + NUL fails and leaves buf alone. */
	memset(buf, 'x', 8);
	CHECK_ERROR(pn_brahmi_render(1u, buf, 4, &err_), err_, PN_ERROR_BUFFER);
	CHECK(memcmp(buf, "xxxx", 4) == 0);

	/* --- parsing: the round trip, exhaustive -----------------------------
	 *
	 * Whatever the renderer carved, the parser must read back — through
	 * both the NUL-terminated spelling and the exact byte count. */
	for (uint64_t n = 0; n <= PN_BRAHMI_MAX_VALUE; n++) {
		char rendered[3 * 12 + 4 + 1];
		size_t need = pn_brahmi_render_size(n);
		CHECK_OK(pn_brahmi_render(n, rendered, need + 1, &err_), err_);
		(void)expect_render(n, want);
		CHECK(strcmp(rendered, want) == 0);
		uint64_t back = UINT64_MAX;
		CHECK_OK(pn_brahmi_parse(rendered, strlen(rendered), &back, &err_), err_);
		CHECK(back == n);
		back = UINT64_MAX;
		CHECK_OK(pn_brahmi_parse(rendered, need, &back, &err_), err_);
		CHECK(back == n);
		if (pn_fails)
			break;    /* the first failure names n */
	}

	/* The Sasaram reading, spelled out. */
	uint64_t read;
	const char *sasaram = HUNDRED JOINER TWO FIFTY SIX;
	CHECK_OK(pn_brahmi_parse(sasaram, strlen(sasaram), &read, &err_), err_);
	CHECK(read == 256u);

	/* The Damasena reading. */
	const char *damasena = HUNDRED FIFTY THREE;
	CHECK_OK(pn_brahmi_parse(damasena, strlen(damasena), &read, &err_), err_);
	CHECK(read == 153u);

	/* The additive component: 100 then 4 reads 104, no ligature. */
	CHECK_OK(pn_brahmi_parse(HUNDRED FOUR, 8, &read, &err_), err_);
	CHECK(read == 104u);

	/* The ligatured form reads the product, not the sum. */
	CHECK_OK(pn_brahmi_parse(HUNDRED JOINER FOUR, 12, &read, &err_), err_);
	CHECK(read == 400u);

	/* The blank reads back as the nothing it was. */
	read = UINT64_MAX;
	CHECK_OK(pn_brahmi_parse("", 0, &read, &err_), err_);
	CHECK(read == 0u);

	/* --- errors: the strict canonical form --------------------------------
	 *
	 * Each leaves *out untouched — checked via the sentinel. */
	static const char *const bad_strings[] = {
		" ",                      /* a lone space is not the scribe's blank */
		"abc",                    /* not signs, not blanks                   */
		"\t" ONE,                 /* tabs are not how pillars are carved     */
		"\x22",                   /* an embedded quote byte                  */
		FIFTY HUNDRED,            /* ascending: the hundred leads on the rock*/
		TWO TEN,                  /* ascending within the lower two slots    */
		TEN ONE TEN,              /* a tens term split in two is two terms   */
		ONE TWO,                  /* two words in the ones slot              */
		HUNDRED HUNDRED,          /* a bare power twice — a ligature, scribe */
		THOUSAND THOUSAND,
		HUNDRED THOUSAND,         /* ascending on the pillar                 */
		TEN HUNDRED,              /* the pillar reads top-down               */
		JOINER,                   /* a joiner with nothing joined            */
		JOINER JOINER,            /* doubled                                 */
		JOINER ONE,               /* leading: joining from nothing           */
		ONE JOINER,               /* trailing: joining to nothing            */
		HUNDRED JOINER,           /* a ligature cut off before its multiplier*/
		HUNDRED JOINER ONE,       /* 1 x 100: write the power sign bare      */
		THOUSAND JOINER TWO JOINER ONE,  /* a joiner off its power sign      */
		HUNDRED JOINER JOINER TWO,       /* doubled before the multiplier    */
		HUNDRED JOINER THIRTY,    /* joined to a tens sign, not a multiplier */
		HUNDRED JOINER HUNDRED,   /* joined to a power sign                  */
		HUNDRED JOINER TEN,       /* joined to TEN: not a digits-digit       */
		TWO JOINER THREE,         /* a joiner on an additive sign            */
		HUNDRED FIVE FIVE,        /* twice in the ones slot (105+5 is a slip)*/
		FORTY FIFTY,              /* twice in the tens slot                  */
		THOUSAND ONE HUNDRED TWO, /* order lost below the thousand           */
		HUNDRED FIFTY SIX FIVE,   /* the ones term already spoken            */
	};
	for (size_t i = 0; i < sizeof bad_strings / sizeof bad_strings[0]; i++) {
		read = 256u;                        /* sentinel */
		CHECK_ERROR(pn_brahmi_parse(bad_strings[i], strlen(bad_strings[i]),
				&read, &err_), err_, PN_ERROR_SYNTAX);
		CHECK(read == 256u);                 /* untouched */
	}

	/* Nulls. */
	CHECK_ERROR(pn_brahmi_parse(pn_nothing(), 4, &read, &err_), err_, PN_ERROR_NULL);
	CHECK_ERROR(pn_brahmi_parse(ONE, 4, pn_nothing(), &err_), err_, PN_ERROR_NULL);

	/* A cut-off sign (3 of the ONE sign's 4 bytes) is not a sign. */
	read = 256u;
	CHECK_ERROR(pn_brahmi_parse(ONE, 3, &read, &err_), err_, PN_ERROR_SYNTAX);
	CHECK(read == 256u);

	/* Half a ligature: the joiner's first 3 bytes are foreign. */
	read = 256u;
	CHECK_ERROR(pn_brahmi_parse(HUNDRED JOINER, 4 + 3, &read, &err_), err_, PN_ERROR_SYNTAX);
	CHECK(read == 256u);

	/* A sign glued after a cut-off run: the tail byte is foreign. */
	read = 256u;
	CHECK_ERROR(pn_brahmi_parse(ONE "\x41", 5, &read, &err_), err_, PN_ERROR_SYNTAX);
	CHECK(read == 256u);

	/* A Brahmi DIGIT (U+11066..U+1106F) is not a Brahmi NUMBER: the
	 * joiner is legal only between number signs, and the digits are a
	 * different invention entirely. */
	read = 256u;
	CHECK_ERROR(pn_brahmi_parse("\U00011066", 4, &read, &err_), err_, PN_ERROR_SYNTAX);
	CHECK(read == 256u);

	return CHECK_RUN();
}
