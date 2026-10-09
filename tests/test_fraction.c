/*  test_fraction.c — common fractions: make, the continued fraction
   road from double, parse and format.
   Golden values, each confirmed against Python's
   Fraction(x).limit_denominator(max):
   - pi: 22/7 (max 10, Archimedes), 311/99 (max 100), 355/113
   (max 1000, Zu Chongzhi).
   - sqrt(2) with max 100 is 140/99 - a semiconvergent, which the
   plain convergent 99/70 loses to.
   - YBC 7289's 1;24,51,10 is exactly 30547/21600.
*/

#include <stdlib.h>

#include "check.h"

#define CHECK_FRACTION(f, top, bottom, is_neg) \
    CHECK((f).numerator == (top) && (f).denominator == (bottom) && (f).neg == (is_neg))

int main(void) {
    pn_fraction f;
    pn_fraction g;
    char buf[PN_FRACTION_FORMAT_MAX_LEN];

    // MARK: - Make

    f = pn_fraction_make(6, 8, false);
    CHECK_FRACTION(f, 3, 4, false);
    f = pn_fraction_make(6, 8, true);
    CHECK_FRACTION(f, 3, 4, true);
    f = pn_fraction_make(0, 5, true); /* zero carries no sign */
    CHECK_FRACTION(f, 0, 1, false);
    f = pn_fraction_make(7, 1, false);
    CHECK_FRACTION(f, 7, 1, false);
    f = pn_fraction_make(UINT64_MAX, UINT64_MAX, false);
    CHECK_FRACTION(f, 1, 1, false);

    // MARK: - From Double

    const double pi = 3.14159265358979323846;

    f = pn_fraction_from_double(pi, 10);
    CHECK_FRACTION(f, 22, 7, false);
    f = pn_fraction_from_double(pi, 100);
    CHECK_FRACTION(f, 311, 99, false);
    f = pn_fraction_from_double(pi, 1000);
    CHECK_FRACTION(f, 355, 113, false);
    f = pn_fraction_from_double(-pi, 1000);
    CHECK_FRACTION(f, 355, 113, true);
    f = pn_fraction_from_double(2.71828182845904523536, 1000);
    CHECK_FRACTION(f, 1457, 536, false);
    f = pn_fraction_from_double(sqrt(2.0), 100);
    CHECK_FRACTION(f, 140, 99, false);

    f = pn_fraction_from_double(0.75, 1000);
    CHECK_FRACTION(f, 3, 4, false);
    f = pn_fraction_from_double(-0.5, 1000);
    CHECK_FRACTION(f, 1, 2, true);
    f = pn_fraction_from_double(0.1, 1000000);
    CHECK_FRACTION(f, 1, 10, false);
    f = pn_fraction_from_double(1.0 / 3.0, 10);
    CHECK_FRACTION(f, 1, 3, false);
    f = pn_fraction_from_double(3.0, 1000);
    CHECK_FRACTION(f, 3, 1, false);
    f = pn_fraction_from_double(0.0, 1000);
    CHECK_FRACTION(f, 0, 1, false);
    f = pn_fraction_from_double(-0.0, 1000);
    CHECK_FRACTION(f, 0, 1, false);
    f = pn_fraction_from_double(0.6, 1); /* the bound of 1 rounds to a whole */
    CHECK_FRACTION(f, 1, 1, false);
    f = pn_fraction_from_double(2.5, 1); /* a tie goes to the one nearer zero */
    CHECK_FRACTION(f, 2, 1, false);
    f = pn_fraction_from_double(-2.5, 1); /* the same, mirrored */
    CHECK_FRACTION(f, 2, 1, true);
    f = pn_fraction_from_double(1e-300, 1000); /* nearer 0 than 1/1000 */
    CHECK_FRACTION(f, 0, 1, false);
    f = pn_fraction_from_double(1e20, 1); /* past the ledger: saturate */
    CHECK_FRACTION(f, UINT64_MAX, 1, false);

    /* YBC 7289: the sexagesimal reading, through double, back to its exact ratio */
    pn_base60 v;
    double d;

    CHECK_VALUE(v = pn_base60_parse("1;24,51,10", &err_), err_);
    CHECK_OK(pn_base60_to_double(v, &d, &err_), err_);
    f = pn_fraction_from_double(d, 21600);
    CHECK_FRACTION(f, 30547, 21600, false);

    // MARK: - Equals

    CHECK(pn_fraction_equals(pn_fraction_parse("1/3", NULL), pn_fraction_parse("2/6", NULL))
          == true);
    CHECK(pn_fraction_equals(pn_fraction_parse("-1/3", NULL), pn_fraction_parse("1/3", NULL))
          == false);
    CHECK(pn_fraction_equals(pn_fraction_parse("1/3", NULL), pn_fraction_parse("1/4", NULL))
          == false);
    f = (pn_fraction) {.numerator = 2, .denominator = 6, .neg = false}; /* unreduced, by hand */
    g = (pn_fraction) {.numerator = 0, .denominator = 5, .neg = true};
    CHECK(pn_fraction_equals(f, pn_fraction_make(1, 3, false)));
    CHECK(pn_fraction_equals(g, pn_fraction_make(0, 1, false)));
    f = (pn_fraction) {.numerator = UINT64_MAX, .denominator = UINT64_MAX - 1, .neg = false};
    g = (pn_fraction) {.numerator = UINT64_MAX - 1, .denominator = UINT64_MAX - 2, .neg = false};
    CHECK(pn_fraction_equals(f, g) == false); /* products past 2^64 compare exactly */

    // MARK: - To Double

    CHECK_DBL(pn_fraction_to_double(pn_fraction_make(3, 4, false)), 0.75, 0.0);
    CHECK_DBL(pn_fraction_to_double(pn_fraction_make(1, 2, true)), -0.5, 0.0);
    CHECK_DBL(pn_fraction_to_double(pn_fraction_make(355, 113, false)), pi, 3e-7);

    // MARK: - Parse

    CHECK_VALUE(f = pn_fraction_parse("355/113", &err_), err_);
    CHECK_FRACTION(f, 355, 113, false);
    CHECK_VALUE(f = pn_fraction_parse("-6/8", &err_), err_);
    CHECK_FRACTION(f, 3, 4, true);
    CHECK_VALUE(f = pn_fraction_parse("3", &err_), err_);
    CHECK_FRACTION(f, 3, 1, false);
    CHECK_VALUE(f = pn_fraction_parse("-0/7", &err_), err_);
    CHECK_FRACTION(f, 0, 1, false);
    CHECK_VALUE(f = pn_fraction_parse("18446744073709551615/1", &err_), err_);
    CHECK_FRACTION(f, UINT64_MAX, 1, false);

    CHECK_VALUE_ERROR(f = pn_fraction_parse("", &err_), err_, PN_ERROR_SYNTAX);
    CHECK_VALUE_ERROR(f = pn_fraction_parse("-", &err_), err_, PN_ERROR_SYNTAX);
    CHECK_VALUE_ERROR(f = pn_fraction_parse("1/", &err_), err_, PN_ERROR_SYNTAX);
    CHECK_VALUE_ERROR(f = pn_fraction_parse("/2", &err_), err_, PN_ERROR_SYNTAX);
    CHECK_VALUE_ERROR(f = pn_fraction_parse("1//2", &err_), err_, PN_ERROR_SYNTAX);
    CHECK_VALUE_ERROR(f = pn_fraction_parse("1/2/3", &err_), err_, PN_ERROR_SYNTAX);
    CHECK_VALUE_ERROR(f = pn_fraction_parse(" 1/2", &err_), err_, PN_ERROR_SYNTAX);
    CHECK_VALUE_ERROR(f = pn_fraction_parse("+1/2", &err_), err_, PN_ERROR_SYNTAX);
    CHECK_VALUE_ERROR(f = pn_fraction_parse("1.5", &err_), err_, PN_ERROR_SYNTAX);
    CHECK_VALUE_ERROR(f = pn_fraction_parse("18446744073709551616", &err_), err_, PN_ERROR_RANGE);
    CHECK_VALUE_ERROR(f = pn_fraction_parse("1/18446744073709551616", &err_), err_, PN_ERROR_RANGE);
    CHECK_VALUE_ERROR(f = pn_fraction_parse("1/0", &err_), err_, PN_ERROR_DOMAIN);

    // MARK: - Format

    CHECK_OK(pn_fraction_format_into(pn_fraction_make(355, 113, false), buf, sizeof buf, &err_),
             err_);
    CHECK(strcmp(buf, "355/113") == 0);
    CHECK_OK(pn_fraction_format_into(pn_fraction_make(1, 2, true), buf, sizeof buf, &err_), err_);
    CHECK(strcmp(buf, "-1/2") == 0);
    CHECK_OK(pn_fraction_format_into(pn_fraction_make(3, 1, false), buf, sizeof buf, &err_), err_);
    CHECK(strcmp(buf, "3") == 0);

    /* the widest value fills the declared maximum exactly */
    f = (pn_fraction) {.numerator = UINT64_MAX, .denominator = UINT64_MAX - 1, .neg = true};
    CHECK_OK(pn_fraction_format_into(f, buf, sizeof buf, &err_), err_);
    CHECK(strlen(buf) + 1 == PN_FRACTION_FORMAT_MAX_LEN);

    /* a short buffer is refused and left untouched */
    char small[4] = {'x', 'x', 'x', 'x'};

    CHECK_ERROR(
        pn_fraction_format_into(pn_fraction_make(355, 113, false), small, sizeof small, &err_),
        err_, PN_ERROR_BUFFER);
    CHECK(memcmp(small, "xxxx", sizeof small) == 0);

    f = (pn_fraction) {.numerator = 1, .denominator = 0, .neg = false};
    CHECK_ERROR(pn_fraction_format_into(f, buf, sizeof buf, &err_), err_, PN_ERROR_DOMAIN);

    char* text = NULL;

    CHECK_OK(text = pn_fraction_format(pn_fraction_make(22, 7, false), &err_), err_);
    CHECK(text && strcmp(text, "22/7") == 0);
    free(text);

    // MARK: - Round Trips

    for (uint64_t den = 1; den <= 200; den++) {
        for (uint64_t num = 0; num <= 200; num++) {
            f = pn_fraction_make(num, den, (num & 1) != 0);
            CHECK_OK(pn_fraction_format_into(f, buf, sizeof buf, &err_), err_);
            CHECK_VALUE(g = pn_fraction_parse(buf, &err_), err_);
            CHECK(pn_fraction_equals(f, g));
            g = pn_fraction_from_double(pn_fraction_to_double(f), 200);
            CHECK(pn_fraction_equals(f, g));
        }
        if (pn_fails) {
            break; /* the first failure names den */
        }
    }

    return CHECK_RUN();
}
