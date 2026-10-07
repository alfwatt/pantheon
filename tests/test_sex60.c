/*  test_sex60.c — sexagesimal parse/format/convert.
   Golden values come from the tablets themselves, via:
   - YBC 7289's sqrt(2):      1;24,51,10
   https://en.wikipedia.org/wiki/Sexagesimal
   - Plimpton 322 row 1:      1;59,0,15  (= 28561/14400, sec^2 of the
   119-120-169 triple)
   https://en.wikipedia.org/wiki/Plimpton_322
   - Wikipedia's worked example 8583 = 2*60^2 + 23*60 + 3.
*/

#include "check.h"

int main(void) {
    pn_sex60 v;
    pn_sex60 w;
    double d;
    uint64_t u;
    char buf[PN_SEX60_FORMAT_MAX_LEN];

    /* the tablets */

    /* YBC 7289: the diagonal of a unit square. */
    CHECK_OK(pn_sex60_parse("1;24,51,10", &v, &err_), err_);
    static const uint8_t ybc[] = {1, 24, 51, 10};
    CHECK(v.n_places == 4 && v.radix == 1 && !v.neg);
    CHECK(memcmp(v.places, ybc, sizeof ybc) == 0);
    CHECK_OK(pn_sex60_to_double(v, &d, &err_), err_);
    CHECK_DBL(d, 1.4142129629629630, 1e-15);

    /* Plimpton 322 row 1. */
    CHECK_OK(pn_sex60_parse("1;59,0,15", &v, &err_), err_);
    CHECK_OK(pn_sex60_to_double(v, &d, &err_), err_);
    CHECK_DBL(d, 28561.0 / 14400.0, 1e-15);

    /* Wikipedia's worked example: 8583 = 2*60^2 + 23*60 + 3. */
    CHECK_OK(pn_sex60_from_u64(8583, &v, &err_), err_);
    CHECK_PLACES(v, 3, 2, 23, 3);
    CHECK_OK(pn_sex60_to_u64(v, &u, &err_), err_);
    CHECK(u == 8583);

    /* integers */

    /* Separators ',' and ':' agree; no ';' means period-style integer. */
    CHECK_OK(pn_sex60_parse("1,24,51,10", &v, &err_), err_);
    CHECK_OK(pn_sex60_parse("1:24:51:10", &w, &err_), err_);
    CHECK(v.n_places == 4 && v.radix == 4);
    CHECK(memcmp(&v, &w, sizeof v) == 0);
    CHECK_OK(pn_sex60_to_double(v, &d, &err_), err_);
    CHECK_DBL(d, 305470.0, 0.0); /* 1*60^3 + 24*60^2 + 51*60 + 10 */

    CHECK_OK(pn_sex60_from_u64(0, &v, &err_), err_);
    CHECK_PLACES(v, 1, 0);
    CHECK_OK(pn_sex60_to_u64(v, &u, &err_), err_);
    CHECK(u == 0);

    /* The ambiguous sixty: what a scribe wrote as one lone wedge. */
    CHECK_OK(pn_sex60_from_u64(60, &v, &err_), err_);
    CHECK_PLACES(v, 2, 1, 0);

    CHECK_OK(pn_sex60_from_u64(216000, &v, &err_), err_); /* 60^3 */
    CHECK_PLACES(v, 4, 1, 0, 0, 0);

    /*  Largest value a sexagesimal integer can hold in uint64: 60^10
	   is the last exact power, so test the overflow guard just past it
	   with sixteen 59s (60^16 - 1 > 2^64).
	*/
    static const uint8_t maxes[PN_SEX60_MAX_PLACES] = {59, 59, 59, 59, 59, 59, 59, 59,
                                                       59, 59, 59, 59, 59, 59, 59, 59};
    v = (pn_sex60) {0};
    memcpy(v.places, maxes, sizeof maxes);
    v.n_places = PN_SEX60_MAX_PLACES;
    v.radix = PN_SEX60_MAX_PLACES;
    CHECK_ERROR(pn_sex60_to_u64(v, &u, &err_), err_, PN_ERROR_RANGE);

    /* Fractional places are not integers. */
    CHECK_OK(pn_sex60_parse("0;45", &v, &err_), err_);
    CHECK(v.n_places == 2 && v.radix == 1);
    CHECK_ERROR(pn_sex60_to_u64(v, &u, &err_), err_, PN_ERROR_DOMAIN);
    CHECK_OK(pn_sex60_to_double(v, &d, &err_), err_);
    CHECK_DBL(d, 0.75, 0.0); /* one exact division: 45/60 */

    /* from_double */

    CHECK_OK(pn_sex60_from_double(0.75, 1, &v, &err_), err_);
    CHECK_PLACES(v, 0, 45); /* {45} radix 0: 0;45 = 3/4 */
    CHECK(v.radix == 0);
    CHECK_OK(pn_sex60_to_double(v, &d, &err_), err_);
    CHECK_DBL(d, 0.75, 0.0);

    CHECK_OK(pn_sex60_from_double(1.5, 1, &v, &err_), err_);
    CHECK_PLACES(v, 1, 1, 30);
    CHECK_OK(pn_sex60_to_double(v, &d, &err_), err_);
    CHECK_DBL(d, 1.5, 0.0);

    /*  Round half away from zero at the last kept place, carried all the
	   way out: 0.99999 with two places is 3599.964/3600, which rounds to
	   a whole 1 — the carry chain empties behind it.
	*/
    CHECK_OK(pn_sex60_from_double(0.99999, 2, &v, &err_), err_);
    CHECK(v.n_places == 3 && v.radix == 1 && !v.neg);
    CHECK(v.places[0] == 1 && v.places[1] == 0 && v.places[2] == 0);
    CHECK_OK(pn_sex60_to_double(v, &d, &err_), err_);
    CHECK_DBL(d, 0.99999, 6e-5);

    /* Trailing zero places are KEPT for determinism. */
    CHECK_OK(pn_sex60_from_double(1.0, 2, &v, &err_), err_);
    CHECK(v.n_places == 3 && v.radix == 1);
    CHECK(v.places[0] == 1 && v.places[1] == 0 && v.places[2] == 0);

    CHECK_OK(pn_sex60_from_double(0.0, 0, &v, &err_), err_);
    CHECK(v.n_places == 1 && v.radix == 0 && v.places[0] == 0);

    /* NaN and infinity have no places. */
    CHECK_ERROR(pn_sex60_from_double(NAN, 1, &v, &err_), err_, PN_ERROR_DOMAIN);
    CHECK_ERROR(pn_sex60_from_double(INFINITY, 1, &v, &err_), err_, PN_ERROR_DOMAIN);
    CHECK_ERROR(pn_sex60_from_double(1.0, PN_SEX60_MAX_PLACES + 1, &v, &err_), err_,
                PN_ERROR_DOMAIN);

    /* formatting */

    /*  The YBC 7289 reading round-trips byte-identically, whatever the
	   ordinary separator is: the radix boundary is always ';'.
	*/
    CHECK_OK(pn_sex60_parse("1;24,51,10", &v, &err_), err_);
    for (const char* seps = ",:"; *seps; seps++) {
        char want[PN_SEX60_FORMAT_MAX_LEN];
        snprintf(want, sizeof want, "1;24%c51%c10", *seps, *seps);
        CHECK_OK(pn_sex60_format(v, *seps, buf, sizeof buf, &err_), err_);
        CHECK(strcmp(buf, want) == 0);
        CHECK_OK(pn_sex60_parse(buf, &w, &err_), err_);
        CHECK(memcmp(&v, &w, sizeof v) == 0);
    }
    /*  With ';' as the ordinary separator a fractional value formats to
	   two ';' and no longer parses back — documented in the header.
	*/
    CHECK_OK(pn_sex60_format(v, ';', buf, sizeof buf, &err_), err_);
    CHECK(strcmp(buf, "1;24;51;10") == 0);
    CHECK_ERROR(pn_sex60_parse(buf, &w, &err_), err_, PN_ERROR_SYNTAX);

    /*  Period-style integers: first place without its leading zero,
	   later places two digits ("3" is written "03").
	*/
    CHECK_OK(pn_sex60_from_u64(8583, &v, &err_), err_);
    CHECK_OK(pn_sex60_format(v, ',', buf, sizeof buf, &err_), err_);
    CHECK(strcmp(buf, "2,23,03") == 0);

    /* Negatives round-trip. */
    CHECK_OK(pn_sex60_parse("-1;6", &v, &err_), err_);
    CHECK(v.neg);
    CHECK_OK(pn_sex60_to_double(v, &d, &err_), err_);
    CHECK_DBL(d, -1.1, 1e-15); /* -(1 + 6/60) */
    CHECK_OK(pn_sex60_format(v, ',', buf, sizeof buf, &err_), err_);
    CHECK(strcmp(buf, "-1;06") == 0); /* later places: two digits */

    /*  the error table
	   Grammar failures: every one of these must leave *out untouched, too
	   (checked once via a sentinel).
	*/
    static const char* const syntax_bad[] = {"",   "-",  ";",  "1;",  "1,,2", "1;2;3", "+1",
                                             " 1", "1 ", "1a", ";45", "1;2,", "1;"};
    for (size_t i = 0; i < sizeof syntax_bad / sizeof syntax_bad[0]; i++) {
        v = (pn_sex60) {0};
        v.n_places = 7; /* sentinel */
        CHECK_ERROR(pn_sex60_parse(syntax_bad[i], &v, &err_), err_, PN_ERROR_SYNTAX);
        CHECK(v.n_places == 7); /* untouched */
    }

    /*  Range failures: a place over 59, a run of three digits, a
	   seventeenth place.
	*/
    CHECK_ERROR(pn_sex60_parse("60", &v, &err_), err_, PN_ERROR_RANGE);
    CHECK_ERROR(pn_sex60_parse("3599", &v, &err_), err_, PN_ERROR_RANGE); /* places 35, 99 */
    CHECK_ERROR(pn_sex60_parse("1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1", &v, &err_), err_,
                PN_ERROR_RANGE);

    /* Nulls everywhere. */
    CHECK_ERROR(pn_sex60_parse(pn_nothing(), &v, &err_), err_, PN_ERROR_NULL);
    CHECK_ERROR(pn_sex60_parse("1", pn_nothing(), &err_), err_, PN_ERROR_NULL);
    CHECK_ERROR(pn_sex60_from_u64(1, pn_nothing(), &err_), err_, PN_ERROR_NULL);
    CHECK_ERROR(pn_sex60_to_u64(v, pn_nothing(), &err_), err_, PN_ERROR_NULL);
    CHECK_ERROR(pn_sex60_to_double(v, pn_nothing(), &err_), err_, PN_ERROR_NULL);
    CHECK_ERROR(pn_sex60_from_double(1.0, 1, pn_nothing(), &err_), err_, PN_ERROR_NULL);
    CHECK_ERROR(pn_sex60_format(v, ',', pn_nothing(), sizeof buf, &err_), err_, PN_ERROR_NULL);

    /* Bad separator character; malformed values. */
    CHECK_OK(pn_sex60_parse("1", &v, &err_), err_);
    CHECK_ERROR(pn_sex60_format(v, '.', buf, sizeof buf, &err_), err_, PN_ERROR_SYNTAX);
    w = v;
    w.n_places = 0;
    CHECK_ERROR(pn_sex60_format(w, ',', buf, sizeof buf, &err_), err_, PN_ERROR_DOMAIN);
    w = v;
    w.places[0] = 60;
    CHECK_ERROR(pn_sex60_format(w, ',', buf, sizeof buf, &err_), err_, PN_ERROR_DOMAIN);
    w = v;
    w.radix = 2; /* > n_places */
    CHECK_ERROR(pn_sex60_to_u64(w, &u, &err_), err_, PN_ERROR_DOMAIN);

    /* Too-small buffer fails and leaves buf byte-for-byte untouched. */
    char small[11];
    memset(small, 'x', sizeof small);
    CHECK_OK(pn_sex60_parse("1;24,51,10", &v, &err_), err_); /* renders to 10 bytes */
    CHECK_ERROR(pn_sex60_format(v, ',', small, 10, &err_), err_, PN_ERROR_BUFFER);
    CHECK(memcmp(small, "xxxxxxxx", 8) == 0);
    CHECK_OK(pn_sex60_format(v, ',', small, sizeof small, &err_), err_);
    CHECK(strcmp(small, "1;24,51,10") == 0);

    return CHECK_RUN();
}
