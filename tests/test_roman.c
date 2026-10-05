/* test_roman.c — Roman numerals, the canonical tallied form.
 *
 * The seven letters I V X L C D M are the signs — pure ASCII, no
 * universal character names needed.  Every expected string below came
 * from the generator .hermes/tmp/gen_test_roman.py via truth(), the
 * per-digit row law (thousands + hundreds row + tens row + units row),
 * cross-checked against the greedy law over the whole domain; none was
 * hand-typed.
 */

#include "check.h"

/* the per-digit rows: index is the digit */
static const char *const th_row[4] = {
	"",
	"M",
	"MM",
	"MMM"
};
static const char *const h_row[10] = {
	"",
	"C",
	"CC",
	"CCC",
	"CD",
	"D",
	"DC",
	"DCC",
	"DCCC",
	"CM"
};
static const char *const t_row[10] = {
	"",
	"X",
	"XX",
	"XXX",
	"XL",
	"L",
	"LX",
	"LXX",
	"LXXX",
	"XC"
};
static const char *const u_row[10] = {
	"",
	"I",
	"II",
	"III",
	"IV",
	"V",
	"VI",
	"VII",
	"VIII",
	"IX"
};

static void render_ok(uint64_t n, char *buf, size_t buflen)
{
	size_t need = pn_roman_render_size(n);
	CHECK_ST(pn_roman_render(n, buf, buflen), pn_ok);
	CHECK(strlen(buf) == need);
}

static void expect_parse(const char *cord, uint64_t want)
{
	uint64_t v = 0xA5A5u;
	CHECK_ST(pn_roman_parse(cord, strlen(cord), &v), pn_ok);
	CHECK(v == want);
}

int main(void)
{
	char buf[64];
	uint64_t v;

	/* --- the row tables the numerals are built from ------------------ */
	CHECK(strcmp(th_row[3], "MMM") == 0);
	CHECK(strcmp(h_row[4], "CD") == 0);
	CHECK(strcmp(h_row[9], "CM") == 0);
	CHECK(strcmp(t_row[4], "XL") == 0);
	CHECK(strcmp(t_row[9], "XC") == 0);
	CHECK(strcmp(u_row[4], "IV") == 0);
	CHECK(strcmp(u_row[9], "IX") == 0);
	CHECK(strcmp(th_row[0], "") == 0);

	/* --- golden numerals (the header table, truth()'s spellings) ------ */
	render_ok(1u, buf, sizeof buf);
	CHECK(strcmp(buf, "I") == 0);
	expect_parse("I", 1u);

	render_ok(4u, buf, sizeof buf);
	CHECK(strcmp(buf, "IV") == 0);
	expect_parse("IV", 4u);

	render_ok(5u, buf, sizeof buf);
	CHECK(strcmp(buf, "V") == 0);
	expect_parse("V", 5u);

	render_ok(9u, buf, sizeof buf);
	CHECK(strcmp(buf, "IX") == 0);
	expect_parse("IX", 9u);

	render_ok(14u, buf, sizeof buf);
	CHECK(strcmp(buf, "XIV") == 0);
	expect_parse("XIV", 14u);

	render_ok(44u, buf, sizeof buf);
	CHECK(strcmp(buf, "XLIV") == 0);
	expect_parse("XLIV", 44u);

	render_ok(90u, buf, sizeof buf);
	CHECK(strcmp(buf, "XC") == 0);
	expect_parse("XC", 90u);

	render_ok(400u, buf, sizeof buf);
	CHECK(strcmp(buf, "CD") == 0);
	expect_parse("CD", 400u);

	render_ok(900u, buf, sizeof buf);
	CHECK(strcmp(buf, "CM") == 0);
	expect_parse("CM", 900u);

	render_ok(1000u, buf, sizeof buf);
	CHECK(strcmp(buf, "M") == 0);
	expect_parse("M", 1000u);

	render_ok(1999u, buf, sizeof buf);
	CHECK(strcmp(buf, "MCMXCIX") == 0);
	expect_parse("MCMXCIX", 1999u);

	render_ok(3888u, buf, sizeof buf);
	CHECK(strcmp(buf, "MMMDCCCLXXXVIII") == 0);
	expect_parse("MMMDCCCLXXXVIII", 3888u);

	render_ok(3999u, buf, sizeof buf);
	CHECK(strcmp(buf, "MMMCMXCIX") == 0);
	expect_parse("MMMCMXCIX", 3999u);

	/* --- exhaustive round trip over the whole domain ------------------ */
	for (uint64_t n = 0; n <= PN_ROMAN_MAX_VALUE; n++) {
		char b2[64];
		size_t need = pn_roman_render_size(n);
		CHECK_ST(pn_roman_render(n, b2, sizeof b2), pn_ok);
		CHECK(strlen(b2) == need);
		uint64_t back = 0;
		CHECK_ST(pn_roman_parse(b2, strlen(b2), &back), pn_ok);
		CHECK(back == n);
			/* the exact-fit buffer, and one byte short of the NUL */
		CHECK_ST(pn_roman_render(n, buf, need + 1u), pn_ok);
			}

	/* --- the ceiling and its neighbours -------------------------------- */
	render_ok(PN_ROMAN_MAX_VALUE, buf, sizeof buf);
	CHECK(strcmp(buf, "MMMCMXCIX") == 0);
	render_ok(PN_ROMAN_MAX_VALUE - 1u, buf, sizeof buf);
	CHECK(strcmp(buf, "MMMCMXCVIII") == 0);

	/* --- near-miss battery: the grammar the abacist settled on ---------- */
	CHECK_ST(pn_roman_parse("IIII", 4u, &v), pn_err_syntax);	/* the clock maker's fourth I */
	CHECK_ST(pn_roman_parse("XXXX", 4u, &v), pn_err_syntax);	/* a fourth X */
	CHECK_ST(pn_roman_parse("CCCC", 4u, &v), pn_err_syntax);	/* a fourth C */
	CHECK_ST(pn_roman_parse("MMMM", 4u, &v), pn_err_syntax);	/* a fourth M */
	CHECK_ST(pn_roman_parse("VL", 2u, &v), pn_err_syntax);	/* V may not subtract */
	CHECK_ST(pn_roman_parse("IC", 2u, &v), pn_err_syntax);	/* unit leaps past its decade */
	CHECK_ST(pn_roman_parse("XM", 2u, &v), pn_err_syntax);	/* X may not undershoot M */
	CHECK_ST(pn_roman_parse("IIX", 3u, &v), pn_err_syntax);	/* two units before a ten */
	CHECK_ST(pn_roman_parse("IXI", 3u, &v), pn_err_syntax);	/* tokens out of order */
	CHECK_ST(pn_roman_parse("XVX", 3u, &v), pn_err_syntax);	/* a decade reshuffled */
	CHECK_ST(pn_roman_parse("ix", 2u, &v), pn_err_syntax);	/* lowercase — the abacist's heresy */
	CHECK_ST(pn_roman_parse("MCMXCix", 7u, &v), pn_err_syntax);	/* lowercase tail */
	CHECK_ST(pn_roman_parse("MMMDCCCLXXXVIIIA", 16u, &v), pn_err_syntax);	/* junk past the last I */
	CHECK_ST(pn_roman_parse("M M", 3u, &v), pn_err_syntax);	/* a space inside the tally */
	CHECK_ST(pn_roman_parse("I\xc2\xb9", 3u, &v), pn_err_syntax);	/* the superscript-one impostor U+00B9 */
	CHECK_ST(pn_roman_parse("\xe2\x85\xa0", 3u, &v), pn_err_syntax);	/* the Roman numeral block glyph U+2160 */
	CHECK_ST(pn_roman_parse("\xce\x99", 2u, &v), pn_err_syntax);	/* a Greek capital iota wearing I's coat */
	CHECK_ST(pn_roman_parse("MMMM", 4u, &v), pn_err_syntax);	/* past MMM the ceiling: also ungrammatical */
	CHECK_ST(pn_roman_parse("IVI", 3u, &v), pn_err_syntax);	/* digraph then its own unit again */
	CHECK_ST(pn_roman_parse("CMCM", 4u, &v), pn_err_syntax);	/* the hundreds subtracted twice */
	CHECK_ST(pn_roman_parse("XCX", 3u, &v), pn_err_syntax);	/* the tens subtracted twice */
	CHECK_ST(pn_roman_parse("IXIX", 4u, &v), pn_err_syntax);	/* the units subtracted twice */
	CHECK_ST(pn_roman_parse("VIV", 3u, &v), pn_err_syntax);	/* V then IV: not descending */
	CHECK_ST(pn_roman_parse("MDCCCCLXXXXVIIII", 16u, &v), pn_err_syntax);	/* the un-abacised long form: four each */
	CHECK_ST(pn_roman_parse("\x01", 1u, &v), pn_err_syntax);	/* control byte */
	CHECK_ST(pn_roman_parse("I\x00x", 3u, &v), pn_err_syntax);	/* NUL inside the stated buflen */

	/* --- legal tallies the goldens do not show -------------------------- */
	expect_parse("", 0u);	/* 0 */
	expect_parse("MMMDCCCLXXXVIII", 3888u);	/* 3888 */
	expect_parse("MCMXCIX", 1999u);	/* 1999 */
	expect_parse("XLIX", 49u);	/* 49 */
	expect_parse("CDXCIX", 499u);	/* 499 */
	expect_parse("MMMCMXCIX", 3999u);	/* 3999 */
	expect_parse("MMM", 3000u);	/* 3000 */
	expect_parse("DCCC", 800u);	/* 800 */
	expect_parse("LXXX", 80u);	/* 80 */
	expect_parse("VIII", 8u);	/* 8 */

	/* --- the blanks honor zero (Bede's nulla) ---------------------------- */
	render_ok(0u, buf, sizeof buf);
	 CHECK(buf[0] == '\0');
	CHECK(pn_roman_render_size(0u) == 0u);
	v = 0xA5A5u;
	CHECK_ST(pn_roman_parse("", 0, &v), pn_ok);
	CHECK(v == 0u);

	/* --- range -------------------------------------------------------------*/
	CHECK_ST(pn_roman_render(PN_ROMAN_MAX_VALUE + 1u, buf, sizeof buf),
	         pn_err_range);
	CHECK_ST(pn_roman_render(UINT64_MAX, buf, sizeof buf), pn_err_range);

	/* --- buffer -------------------------------------------------------------*/
	size_t fifteen = pn_roman_render_size(3888u);
	CHECK(fifteen == 15u);
	buf[0] = '?';
	CHECK_ST(pn_roman_render(3888u, buf, fifteen), pn_err_buffer);
	CHECK(buf[0] == '?');

	/* --- nulls ---------------------------------------------------------------*/
	CHECK_ST(pn_roman_render(5u, NULL, sizeof buf), pn_err_null);
	CHECK_ST(pn_roman_parse(NULL, 0, &v), pn_err_null);
	CHECK_ST(pn_roman_parse("VIII", 4, NULL), pn_err_null);

	return CHECK_RUN();
}
