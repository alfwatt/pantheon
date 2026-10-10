/*  test_greek.c — Greek alphabetic (Milesian/Ionic) numerals, transcribed.
   Signs as C11 universal character names (this file stays pure ASCII):
   the 27 numeral signs — 9 units alpha..theta with stigma U+03DB for
   6, 9 tens iota..koppa (koppa U+03DF for 90), 9 hundreds rho..omega
   with sampi U+03E1 for 900 and plain sigma for 200 — plus LOWK
   U+0375, the low keraia announcing the thousands run, and DEXIA
   U+0374, the trailing keraia every nonempty cord wears.
   The separator space: 10001 (thousands run ι | units run α) and
   11000 (one thousands run ια) would butt onto the same cord — 972
   such collisions live in the domain — so a number whose two runs are
   both alive separates them with ONE space and the parser demands it
   exactly there. Every expected cord below was spelled by the
   generator .hermes/tmp/gen_test_greek.py from the truth() law,
   verified bijective over 0..999999; none was hand-typed.
*/

#include "check.h"

#define LOWK  "\u0375"
#define DEXIA "\u0374"
#define SEP   " "

#define ALPHA   "\u03b1"
#define BETA    "\u03b2"
#define GAMMA   "\u03b3"
#define DELTA   "\u03b4"
#define EPSILON "\u03b5"
#define STIGMA  "\u03db"
#define ZETA    "\u03b6"
#define ETA     "\u03b7"
#define THETA   "\u03b8"
#define IOTA    "\u03b9"
#define KAPPA   "\u03ba"
#define LAMDA   "\u03bb"
#define MU      "\u03bc"
#define NU      "\u03bd"
#define XI      "\u03be"
#define OMICRON "\u03bf"
#define PI      "\u03c0"
#define KOPPA   "\u03df"
#define RHO     "\u03c1"
#define SIGMA_  "\u03c3"
#define TAU     "\u03c4"
#define UPSILON "\u03c5"
#define PHI     "\u03c6"
#define CHI     "\u03c7"
#define PSI     "\u03c8"
#define OMEGA   "\u03c9"
#define SAMPI   "\u03e1"

/*  d signs of each row, per digit 0..9; index 0 is the row's empty
   place - the digit that writes nothing there.
*/
static const char* const units_row[10] = {"",      ALPHA,  BETA, GAMMA, DELTA,
                                          EPSILON, STIGMA, ZETA, ETA,   THETA};
static const char* const tens_row[10] = {"", IOTA, KAPPA, LAMDA, MU, NU, XI, OMICRON, PI, KOPPA};
static const char* const hund_row[10] = {"",  RHO, SIGMA_, TAU,   UPSILON,
                                         PHI, CHI, PSI,    OMEGA, SAMPI};

/* Render into a generous buffer, asserting render_size == strlen. */
static void render_ok(uint64_t n, char* buf, size_t buflen) {
    size_t need = pn_greek_render_size(n);
    CHECK_OK(pn_greek_render(n, buf, buflen, &err_), err_);
    CHECK(strlen(buf) == need);
}

/* Parse a cord (its own length) and demand the value. */
static void expect_parse(const char* cord, uint64_t want) {
    uint64_t v = 0xA5A5u;
    CHECK_VALUE(v = pn_greek_parse(cord, &err_), err_);
    CHECK(v == want);
}

int main(void) {
    char buf[64];
    uint64_t v;

    /* the two keraia and their raw bytes */
    render_ok(1u, buf, sizeof buf);
    CHECK(strcmp(buf, ALPHA DEXIA) == 0);
    /*  the dexia's canonical bytes pinned: CD B4 — NFD unfolds it to
	   U+02B9 and NFC never brings it back, so the module owns them
	*/
    CHECK((unsigned char) buf[2] == 0xCD && (unsigned char) buf[3] == 0xB4);
    render_ok(1000u, buf, sizeof buf);
    /* the low keraia: CD B5, NFC-stable */
    CHECK((unsigned char) buf[0] == 0xCD && (unsigned char) buf[1] == 0xB5);

    /* the separator: the collision the space exists to kill */
    render_ok(10001u, buf, sizeof buf);
    CHECK(strcmp(buf, LOWK IOTA SEP ALPHA DEXIA) == 0);
    render_ok(11000u, buf, sizeof buf);
    CHECK(strcmp(buf, LOWK IOTA ALPHA DEXIA) == 0);
    /* same letter in both runs: the space, not the letters, separates */
    render_ok(1001u, buf, sizeof buf);
    CHECK(strcmp(buf, LOWK ALPHA SEP ALPHA DEXIA) == 0);

    /* golden cords (truth()'s spellings, see the header table) */
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

    /* the row tables the cords are cut from */
    CHECK(strcmp(units_row[6], STIGMA) == 0);
    CHECK(strcmp(tens_row[9], KOPPA) == 0);
    CHECK(strcmp(hund_row[2], SIGMA_) == 0);
    CHECK(strcmp(hund_row[9], SAMPI) == 0);
    CHECK(units_row[0][0] == '\0' && tens_row[0][0] == '\0' && hund_row[0][0] == '\0');

    /* exhaustive round trip: render then parse, the whole domain */
    for (uint64_t n = 0; n <= PN_GREEK_MAX_VALUE; n++) {
        char b2[64];
        size_t need = pn_greek_render_size(n);
        CHECK_OK(pn_greek_render(n, b2, sizeof b2, &err_), err_);
        CHECK(strlen(b2) == need);
        /* the exact-fit buffer */
        CHECK_OK(pn_greek_render(n, buf, need + 1u, &err_), err_);
        uint64_t back = 0;
        CHECK_VALUE(back = pn_greek_parse(b2, &err_), err_);
        CHECK(back == n);
    }

    /* deep sweep d * 10^k over the keraia decades */
    for (uint64_t d = 1; d <= 9; d++) {
        for (unsigned k = 2; k <= 5; k++) {
            uint64_t p = 1;
            for (unsigned j = 0; j < k; j++) {
                p *= 10u;
            }
            uint64_t n = d * p;
            if (n > PN_GREEK_MAX_VALUE) {
                continue;
            }
            char b2[64];
            CHECK_OK(pn_greek_render(n, b2, sizeof b2, &err_), err_);
            uint64_t back = 0;
            CHECK_VALUE(back = pn_greek_parse(b2, &err_), err_);
            CHECK(back == n);
        }
    }
    /* the neighbours of the ceiling */
    for (uint64_t n = PN_GREEK_MAX_VALUE - 2u; n <= PN_GREEK_MAX_VALUE; n++) {
        char b2[64];
        CHECK_OK(pn_greek_render(n, b2, sizeof b2, &err_), err_);
        uint64_t back = 0;
        CHECK_VALUE(back = pn_greek_parse(b2, &err_), err_);
        CHECK(back == n);
    }

    /* near-miss battery: all the strict cord refuses */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* lone trailing keraia is no number */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xce\xb1\xcd\xb4\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* dexia twice */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xcd\xb4\xce\xb1\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* dexia leads */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xcd\xb5", &err_), err_,
                      PN_ERROR_SYNTAX); /* bare low keraia */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xcd\xb5\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* keraia pair cordoned around nothing */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xce\xb1\xcd\xb5\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* low keraia mid-cord */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xce\xb1\xca\xb9", &err_), err_,
                      PN_ERROR_SYNTAX); /* NFC twin U+02B9 prime, not the U+0374 bytes */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xce\xb1\xce\x84", &err_), err_,
                      PN_ERROR_SYNTAX); /* tonos impostor */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xce\xb2\xce\xb2\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* doubled sign in a run */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xce\xb8\xce\xb1\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* backward pair inside one row */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xce\xb8\xcf\x81\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* backward rows: units before hundreds */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xce\x91\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* capital alpha */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\x41\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* Latin letter */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xe2\x85\xa0\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* Roman numeral glyph I */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xcd\xb5\xcf\xb2\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* lunate sigma for 200 */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xcf\x82\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* final sigma for 200 */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xcf\x9d\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* digamma for 6 */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xcd\xb5\xcf\x98\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* capital koppa for 90 */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xce\xb1\x20\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* space before the trailing keraia */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\x20\xce\xb1\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* space leads the cord */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xcd\xb5\xce\xb1\x20\x20\xce\xb1\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* doubled separator */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xce\xb1\x20\xce\xb2\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* space without the low keraia */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xcd\xb5\x20\xce\xb1\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* space starves the thousands run */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xcd\xb5\xce\xb1\x20\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* space starves the units run */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xc3\x8d", &err_), err_,
                      PN_ERROR_SYNTAX); /* cut-off 2-byte sequence */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xce\xb1\xc3\x8d", &err_), err_,
                      PN_ERROR_SYNTAX); /* truncated dexia bytes */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xcd\xb5\xce\xb1\xce\xb1\xcd\xb4", &err_), err_,
                      PN_ERROR_SYNTAX); /* doubled sign across imagined runs */
    CHECK_VALUE_ERROR(v = pn_greek_parse("\xce\xb1\xca\xb9", &err_), err_,
                      PN_ERROR_SYNTAX); /* render(1) put through NFC: the prime twin is refused */

    /* legal cords the goldens do not show */
    expect_parse("\xcd\xb5\xce\xb1\x20\xce\xb1\xcd\xb4", 1001u);  /* 1001 */
    expect_parse("\xcd\xb5\xce\xb9\x20\xce\xb1\xcd\xb4", 10001u); /* 10001 */
    expect_parse("\xcd\xb5\xce\xb9\xce\xb1\xcd\xb4", 11000u);     /* 11000 */
    expect_parse("\xcd\xb5\xce\xb1\x20\xcf\x81\xcd\xb4", 1100u);  /* 1100 */
    expect_parse("\xcd\xb5\xcf\x81\xcd\xb4", 100000u);            /* 100000 */

    /* the blanks honor zero */
    render_ok(0u, buf, sizeof buf);
    CHECK(buf[0] == '\0');
    CHECK(pn_greek_render_size(0u) == 0u);
    v = 0xA5A5u;
    CHECK_VALUE(v = pn_greek_parse("", &err_), err_);
    CHECK(v == 0u);

    /* range */
    CHECK_ERROR(pn_greek_render(PN_GREEK_MAX_VALUE + 1u, buf, sizeof buf, &err_), err_,
                PN_ERROR_RANGE);
    CHECK_ERROR(pn_greek_render(UINT64_MAX, buf, sizeof buf, &err_), err_, PN_ERROR_RANGE);

    /* buffer */
    size_t nine = pn_greek_render_size(999999u);
    /* one byte short of the NUL — the renderer must not touch buf */
    buf[0] = '?';
    CHECK_ERROR(pn_greek_render(999999u, buf, nine, &err_), err_, PN_ERROR_BUFFER);
    CHECK(buf[0] == '?');

    return CHECK_RUN();
}
