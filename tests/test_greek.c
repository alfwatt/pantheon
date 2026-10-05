/* test_greek.c — Greek alphabetic (Milesian/Ionic) numerals, transcribed.
 *
 * Signs as C11 universal character names (this file stays pure ASCII):
 * the 27 numeral signs — 9 units alpha..theta with stigma U+03DB for
 * 6, 9 tens iota..koppa (koppa U+03DF for 90), 9 hundreds rho..omega
 * with sampi U+03E1 for 900 and plain sigma for 200 — plus LOWK
 * U+0375, the low keraia announcing the thousands run, and DEXIA
 * U+0374, the trailing keraia every nonempty cord wears.
 *
 * The separator space: 10001 (thousands run ι | units run α) and
 * 11000 (one thousands run ια) would butt onto the same cord — 972
 * such collisions live in the domain — so a number whose two runs are
 * both alive separates them with ONE space and the parser demands it
 * exactly there.  Every expected cord below was spelled by the
 * generator .hermes/tmp/gen_test_greek.py from the truth() law,
 * verified bijective over 0..999999; none was hand-typed.
 */

#include "check.h"

#define LOWK     "\u0375"
#define DEXIA    "\u0374"
#define SEP      " "

#define ALPHA     "\u03b1"
#define BETA      "\u03b2"
#define GAMMA     "\u03b3"
#define DELTA     "\u03b4"
#define EPSILON   "\u03b5"
#define STIGMA    "\u03db"
#define ZETA      "\u03b6"
#define ETA       "\u03b7"
#define THETA     "\u03b8"
#define IOTA      "\u03b9"
#define KAPPA     "\u03ba"
#define LAMDA     "\u03bb"
#define MU        "\u03bc"
#define NU        "\u03bd"
#define XI        "\u03be"
#define OMICRON   "\u03bf"
#define PI        "\u03c0"
#define KOPPA     "\u03df"
#define RHO       "\u03c1"
#define SIGMA_    "\u03c3"
#define TAU       "\u03c4"
#define UPSILON   "\u03c5"
#define PHI       "\u03c6"
#define CHI       "\u03c7"
#define PSI       "\u03c8"
#define OMEGA     "\u03c9"
#define SAMPI     "\u03e1"

/* d signs of each row, per digit 0..9; index 0 is the row's empty
    place — the digit that writes nothing there. */
static const char *const units_row[10] = {
	"",
	ALPHA,
	BETA,
	GAMMA,
	DELTA,
	EPSILON,
	STIGMA,
	ZETA,
	ETA,
	THETA
};
static const char *const tens_row[10] = {
	"",
	IOTA,
	KAPPA,
	LAMDA,
	MU,
	NU,
	XI,
	OMICRON,
	PI,
	KOPPA
};
static const char *const hund_row[10] = {
	"",
	RHO,
	SIGMA_,
	TAU,
	UPSILON,
	PHI,
	CHI,
	PSI,
	OMEGA,
	SAMPI
};

/* Render into a generous buffer, asserting render_size == strlen. */
static void render_ok(uint64_t n, char *buf, size_t buflen)
{
	size_t need = pn_greek_render_size(n);
	CHECK_ST(pn_greek_render(n, buf, buflen), pn_ok);
	CHECK(strlen(buf) == need);
}

/* Parse a cord (its own length) and demand the value. */
static void expect_parse(const char *cord, uint64_t want)
{
	uint64_t v = 0xA5A5u;
	CHECK_ST(pn_greek_parse(cord, strlen(cord), &v), pn_ok);
	CHECK(v == want);
}

/* Same, with an explicit buflen (payloads carrying their own bytes). */
static void expect_parse_len(const char *cord, uint64_t want, size_t len)
{
	uint64_t v = 0xA5A5u;
	CHECK_ST(pn_greek_parse(cord, len, &v), pn_ok);
	CHECK(v == want);
}

int main(void)
{
	char buf[64];
	uint64_t v;

	/* --- the two keraia and their raw bytes -------------------------- */
	render_ok(1u, buf, sizeof buf);
	CHECK(strcmp(buf, ALPHA DEXIA) == 0);
	/* the dexia's canonical bytes pinned: CD B4 — NFD unfolds it to
	   U+02B9 and NFC never brings it back, so the module owns them */
	CHECK((unsigned char)buf[1] == 0xCD && (unsigned char)buf[2] == 0xB4);
	render_ok(1000u, buf, sizeof buf);
	/* the low keraia: CD B5, NFC-stable */
	CHECK((unsigned char)buf[0] == 0xCD && (unsigned char)buf[1] == 0xB5);

	/* --- the separator: the collision the space exists to kill -------- */
	render_ok(10001u, buf, sizeof buf);
	CHECK(strcmp(buf, LOWK IOTA SEP ALPHA DEXIA) == 0);
	render_ok(11000u, buf, sizeof buf);
	CHECK(strcmp(buf, LOWK IOTA ALPHA DEXIA) == 0);
	/* same letter in both runs: the space, not the letters, separates */
	render_ok(1001u, buf, sizeof buf);
 CHECK(strcmp(buf, LOWK ALPHA SEP ALPHA DEXIA) == 0);

	/* --- golden cords (truth()'s spellings, see the header table) ----- */
	render_ok(1u, buf, sizeof buf);
	CHECK(strcmp(buf, ALPHA DEXIA) == 0);
	expect_parse(ALPHA DEXIA, 1u);

	render_ok(19u, buf, sizeof buf);
	CHECK(strcmp(buf, IOTA THETA DEXIA) == 0);
	expect_parse(IOTA THETA DEXIA, 19u);

	render_ok(46u, buf, sizeof buf);
	CHECK(strcmp(buf, MU STIGMA DEXIA) == 0);
	expect_parse(MU STIGMA DEXIA, 46u);

	render_ok(256u, buf, sizeof buf);
	CHECK(strcmp(buf, SIGMA_ NU STIGMA DEXIA) == 0);
	expect_parse(SIGMA_ NU STIGMA DEXIA, 256u);

	render_ok(606u, buf, sizeof buf);
	CHECK(strcmp(buf, CHI STIGMA DEXIA) == 0);
	expect_parse(CHI STIGMA DEXIA, 606u);

	render_ok(999u, buf, sizeof buf);
	CHECK(strcmp(buf, SAMPI KOPPA THETA DEXIA) == 0);
	expect_parse(SAMPI KOPPA THETA DEXIA, 999u);

	render_ok(1000u, buf, sizeof buf);
	CHECK(strcmp(buf, LOWK ALPHA DEXIA) == 0);
	expect_parse(LOWK ALPHA DEXIA, 1000u);

	render_ok(1001u, buf, sizeof buf);
	CHECK(strcmp(buf, LOWK ALPHA SEP ALPHA DEXIA) == 0);
	expect_parse(LOWK ALPHA SEP ALPHA DEXIA, 1001u);

	render_ok(2019u, buf, sizeof buf);
	CHECK(strcmp(buf, LOWK BETA SEP IOTA THETA DEXIA) == 0);
	expect_parse(LOWK BETA SEP IOTA THETA DEXIA, 2019u);

	render_ok(10001u, buf, sizeof buf);
	CHECK(strcmp(buf, LOWK IOTA SEP ALPHA DEXIA) == 0);
	expect_parse(LOWK IOTA SEP ALPHA DEXIA, 10001u);

	render_ok(11000u, buf, sizeof buf);
	CHECK(strcmp(buf, LOWK IOTA ALPHA DEXIA) == 0);
	expect_parse(LOWK IOTA ALPHA DEXIA, 11000u);

	render_ok(111000u, buf, sizeof buf);
	CHECK(strcmp(buf, LOWK RHO IOTA ALPHA DEXIA) == 0);
	expect_parse(LOWK RHO IOTA ALPHA DEXIA, 111000u);

	render_ok(999999u, buf, sizeof buf);
	CHECK(strcmp(buf, LOWK SAMPI KOPPA THETA SEP SAMPI KOPPA THETA DEXIA) == 0);
	expect_parse(LOWK SAMPI KOPPA THETA SEP SAMPI KOPPA THETA DEXIA, 999999u);

	/* --- the row tables the cords are cut from ------------------------- */
	CHECK(strcmp(units_row[6], STIGMA) == 0);
	CHECK(strcmp(tens_row[9], KOPPA) == 0);
	CHECK(strcmp(hund_row[2], SIGMA_) == 0);
	CHECK(strcmp(hund_row[9], SAMPI) == 0);
	CHECK(units_row[0][0] == '\0' && tens_row[0][0] == '\0' &&
	      hund_row[0][0] == '\0');

	/* --- exhaustive round trip: render then parse, the whole domain --- */
	for (uint64_t n = 0; n <= PN_GREEK_MAX_VALUE; n++) {
		char b2[64];
		size_t need = pn_greek_render_size(n);
		CHECK_ST(pn_greek_render(n, b2, sizeof b2), pn_ok);
		CHECK(strlen(b2) == need);
		/* the exact-fit buffer */
		CHECK_ST(pn_greek_render(n, buf, need + 1u), pn_ok);
		uint64_t back = 0;
		CHECK_ST(pn_greek_parse(b2, strlen(b2), &back), pn_ok);
		CHECK(back == n);
	}

	/* --- deep sweep d * 10^k over the keraia decades ------------------ */
	for (uint64_t d = 1; d <= 9; d++) {
		for (unsigned k = 2; k <= 5; k++) {
			uint64_t p = 1;
			for (unsigned j = 0; j < k; j++)
				p *= 10u;
			uint64_t n = d * p;
			if (n > PN_GREEK_MAX_VALUE)
				continue;
			char b2[64];
			CHECK_ST(pn_greek_render(n, b2, sizeof b2), pn_ok);
			uint64_t back = 0;
			CHECK_ST(pn_greek_parse(b2, strlen(b2), &back), pn_ok);
			CHECK(back == n);
		}
	}
	/* the neighbours of the ceiling */
	for (uint64_t n = PN_GREEK_MAX_VALUE - 2u; n <= PN_GREEK_MAX_VALUE;
	     n++) {
		char b2[64];
		CHECK_ST(pn_greek_render(n, b2, sizeof b2), pn_ok);
		uint64_t back = 0;
		CHECK_ST(pn_greek_parse(b2, strlen(b2), &back), pn_ok);
		CHECK(back == n);
	}

	/* --- near-miss battery: all the strict cord refuses ---------------- */
	CHECK_ST(pn_greek_parse("\xcd\xb4", 2u, &v), pn_err_syntax);/* lone trailing keraia is no number */
	CHECK_ST(pn_greek_parse("\xce\xb1\xcd\xb4\xcd\xb4", 6u, &v), pn_err_syntax);/* dexia twice */
	CHECK_ST(pn_greek_parse("\xcd\xb4\xce\xb1\xcd\xb4", 6u, &v), pn_err_syntax);/* dexia leads */
	CHECK_ST(pn_greek_parse("\xcd\xb5", 2u, &v), pn_err_syntax);/* bare low keraia */
	CHECK_ST(pn_greek_parse("\xcd\xb5\xcd\xb4", 4u, &v), pn_err_syntax);/* keraia pair cordoned around nothing */
	CHECK_ST(pn_greek_parse("\xce\xb1\xcd\xb5\xcd\xb4", 6u, &v), pn_err_syntax);/* low keraia mid-cord */
	CHECK_ST(pn_greek_parse("\xce\xb1\xca\xb9", 4u, &v), pn_err_syntax);/* NFC twin U+02B9 prime, not the U+0374 bytes */
	CHECK_ST(pn_greek_parse("\xce\xb1\xce\x84", 4u, &v), pn_err_syntax);/* tonos impostor */
	CHECK_ST(pn_greek_parse("\xce\xb2\xce\xb2\xcd\xb4", 6u, &v), pn_err_syntax);/* doubled sign in a run */
	CHECK_ST(pn_greek_parse("\xce\xb8\xce\xb1\xcd\xb4", 6u, &v), pn_err_syntax);/* backward pair inside one row */
	CHECK_ST(pn_greek_parse("\xce\xb8\xcf\x81\xcd\xb4", 6u, &v), pn_err_syntax);/* backward rows: units before hundreds */
	CHECK_ST(pn_greek_parse("\xce\x91\xcd\xb4", 4u, &v), pn_err_syntax);/* capital alpha */
	CHECK_ST(pn_greek_parse("\x41\xcd\xb4", 3u, &v), pn_err_syntax);/* Latin letter */
	CHECK_ST(pn_greek_parse("\xe2\x85\xa0\xcd\xb4", 5u, &v), pn_err_syntax);/* Roman numeral glyph I */
	CHECK_ST(pn_greek_parse("\xcd\xb5\xcf\xb2\xcd\xb4", 6u, &v), pn_err_syntax);/* lunate sigma for 200 */
	CHECK_ST(pn_greek_parse("\xcf\x82\xcd\xb4", 4u, &v), pn_err_syntax);/* final sigma for 200 */
	CHECK_ST(pn_greek_parse("\xcf\x9d\xcd\xb4", 4u, &v), pn_err_syntax);/* digamma for 6 */
	CHECK_ST(pn_greek_parse("\xcd\xb5\xcf\x98\xcd\xb4", 6u, &v), pn_err_syntax);/* capital koppa for 90 */
	CHECK_ST(pn_greek_parse("\xce\xb1\x20\xcd\xb4", 5u, &v), pn_err_syntax);/* space before the trailing keraia */
	CHECK_ST(pn_greek_parse("\x20\xce\xb1\xcd\xb4", 5u, &v), pn_err_syntax);/* space leads the cord */
	CHECK_ST(pn_greek_parse("\xcd\xb5\xce\xb1\x20\x20\xce\xb1\xcd\xb4", 10u, &v), pn_err_syntax);/* doubled separator */
	CHECK_ST(pn_greek_parse("\xce\xb1\x20\xce\xb2\xcd\xb4", 7u, &v), pn_err_syntax);/* space without the low keraia */
	CHECK_ST(pn_greek_parse("\xcd\xb5\x20\xce\xb1\xcd\xb4", 7u, &v), pn_err_syntax);/* space starves the thousands run */
	CHECK_ST(pn_greek_parse("\xcd\xb5\xce\xb1\x20\xcd\xb4", 7u, &v), pn_err_syntax);/* space starves the units run */
	CHECK_ST(pn_greek_parse("\xc3\x8d", 2u, &v), pn_err_syntax);/* cut-off 2-byte sequence */
	CHECK_ST(pn_greek_parse("\xce\xb1\xc3\x8d", 4u, &v), pn_err_syntax);/* truncated dexia bytes */
	CHECK_ST(pn_greek_parse("\xcd\xb5\xce\xb1\xce\xb1\xcd\xb4", 8u, &v), pn_err_syntax);/* doubled sign across imagined runs */
	CHECK_ST(pn_greek_parse("\xce\xb1\xca\xb9", 4u, &v), pn_err_syntax);/* render(1) put through NFC: the prime twin is refused */
	CHECK_ST(pn_greek_parse("\xce\xb1\xcd\xb4\x00\x67\x61\x72\x62\x61\x67\x65", 12u, &v), pn_err_syntax);/* NUL and junk past the cord */

	/* --- legal cords the goldens do not show --------------------------- */
	expect_parse_len("\xcd\xb5\xce\xb1\x20\xce\xb1\xcd\xb4", 1001u, 9u);	/* 1001 */
	expect_parse_len("\xcd\xb5\xce\xb9\x20\xce\xb1\xcd\xb4", 10001u, 9u);	/* 10001 */
	expect_parse_len("\xcd\xb5\xce\xb9\xce\xb1\xcd\xb4", 11000u, 8u);	/* 11000 */
	expect_parse_len("\xcd\xb5\xce\xb1\x20\xcf\x81\xcd\xb4", 1100u, 9u);	/* 1100 */
	expect_parse_len("\xcd\xb5\xcf\x81\xcd\xb4", 100000u, 6u);	/* 100000 */

	/* --- the blanks honor zero ------------------------------------------ */
	render_ok(0u, buf, sizeof buf);
	CHECK(buf[0] == '\0');
	CHECK(pn_greek_render_size(0u) == 0u);
	v = 0xA5A5u;
	CHECK_ST(pn_greek_parse("", 0, &v), pn_ok);
	CHECK(v == 0u);

	/* --- range -----------------------------------------------------------*/
	CHECK_ST(pn_greek_render(PN_GREEK_MAX_VALUE + 1u, buf, sizeof buf),
	         pn_err_range);
	CHECK_ST(pn_greek_render(UINT64_MAX, buf, sizeof buf), pn_err_range);

	/* --- buffer -----------------------------------------------------------*/
	size_t nine = pn_greek_render_size(999999u);
	/* one byte short of the NUL — the renderer must not touch buf */
	buf[0] = '?';
	CHECK_ST(pn_greek_render(999999u, buf, nine), pn_err_buffer);
	CHECK(buf[0] == '?');

	/* --- nulls -------------------------------------------------------------*/
	CHECK_ST(pn_greek_render(5u, NULL, sizeof buf), pn_err_null);
	CHECK_ST(pn_greek_parse(NULL, 0, &v), pn_err_null);
	CHECK_ST(pn_greek_parse(ALPHA DEXIA, 4, NULL), pn_err_null);

	return CHECK_RUN();
}
