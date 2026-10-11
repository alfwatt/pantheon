/*
    pantheon.h — libPantheon: on beyond math.h.

    A C library of number systems and mathematics from history,
    written to teach: Babylonian base 60, Maya base 20, Egyptian,
    Chinese, Brahmi, Inca, Greek and Roman numerals, fractions, and
    early number theory such as Euclid's algorithm and prime testing.
    Each section explains the math and links to further reading.

    The code is also meant to be practical: successful calls never
    allocate memory, values are small structs passed by value, and
    loops are simple and bounded.

    See README.md for the full list of what is included.
*/
#ifndef PANTHEON_H
#define PANTHEON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#pragma clang diagnostic ignored "-Wnullability-extension"

// MARK: - Errors

/*
    A function that can fail takes a last argument `pn_error** error`.
    Set your own `pn_error*` variable to NULL and pass its address.
    If the call succeeds, it stays NULL. If the call fails, the function
    returns NULL (or a zeroed value, for functions that return a struct),
    and *error points to a report explaining what went wrong. Free the
    report with pn_error_free when you are done with it. Pass NULL instead
    of an address if you don't need the explanation.

    An error report says what kind of failure happened (the code)
    and explains it in a sentence (the message). Print it with
    pn_error_describe and free it with pn_error_free.
*/

/// @enum `pn_error_code`: the five kinds of failure
typedef enum pn_error_code {
    PN_ERROR_NULL,   /* something required was missing             */
    PN_ERROR_SYNTAX, /* input text doesn't follow the format       */
    PN_ERROR_RANGE,  /* a number is too big for this system        */
    PN_ERROR_DOMAIN, /* the wrong kind of value for this function  */
    PN_ERROR_BUFFER, /* the caller's buffer is too small            */
} pn_error_code;

/// @struct `pn_error`: the report a failed call leaves behind
/// @field code - the kind of failure, one of the `PN_ERROR_` values above
/// @field message - a sentence describing the error, or NULL when
///     the code says enough
/// @field prior - an earlier error that caused this one, or NULL
typedef struct pn_error {
    pn_error_code code;
    char* _Nullable message;
    struct pn_error* _Nullable prior;
} pn_error;

/// @returns a new `pn_error*` with the
/// @param code provided and a message built from the
/// @param format provided, like printf; free it with pn_error_free
[[nodiscard]]
pn_error* _Nonnull pn_error_init(pn_error_code code, const char* _Nonnull format, ...);

/// Frees the
/// @param error provided; passing NULL is allowed and does nothing
void pn_error_free(pn_error* _Nullable error);

/// Text to print for an error, for example with printf("%s", ...).
///
/// @returns the @param error's message if it has one, the code's
/// name (like "range") if not, and "no error" if @param error is
/// NULL. Never returns NULL. The text belongs to the error, so
/// read it before freeing the error.
const char* _Nonnull pn_error_describe(const pn_error* _Nullable error);

// MARK: - Fractions

/*
    Fractions: a whole-number numerator over a whole-number
    denominator, like 3/4.

    Fractions are older than place-value notation. The Rhind
    papyrus (Egypt, about 1550 BCE) writes 2/7 as 1/4 + 1/28, and
    Greek and Chinese mathematicians approximated pi with fractions:
    Archimedes used 22/7, and Zu Chongzhi (about 480 CE) found
    355/113, correct to six decimal places. Both come from pi's
    continued fraction, 3 + 1/(7 + 1/(15 + 1/(1 + ...))).

    A fraction can hold values that place-value systems can't write
    exactly: 1/7 repeats forever in base 60, and 1/3 repeats in base
    20 and in binary, but as fractions they are just two numbers.

    The sign is stored separately in neg, as in pn_base60, so both
    parts are plain uint64_t values. Fractions from pn_fraction_make
    and pn_fraction_from_double are always in lowest terms with a
    denominator of at least 1, and zero is 0/1 with neg false.
    Compare fractions with pn_fraction_equals, not memcmp: the struct
    has unused padding bytes after neg.

    Fractions are written with ⁄ U+2044 FRACTION SLASH, the character
    Unicode provides for building fractions: many fonts draw 3⁄4 with a
    raised 3 and a lowered 4.  pn_fraction_parse also accepts ∕ U+2215
    DIVISION SLASH (the math operator), ／ U+FF0F FULLWIDTH SOLIDUS (from
    Chinese, Japanese and Korean text) and the plain ASCII / SOLIDUS.

    Further reading:
    - https://en.wikipedia.org/wiki/Fraction
    - https://en.wikipedia.org/wiki/Slash_(punctuation)
    - https://en.wikipedia.org/wiki/Continued_fraction
    - https://en.wikipedia.org/wiki/Rhind_Mathematical_Papyrus
*/

#define PN_FRACTION_SLASH             0x2044u /* ⁄ FRACTION SLASH     UTF-8: E2 81 84 */
#define PN_FRACTION_DIVISION_SLASH    0x2215u /* ∕ DIVISION SLASH     UTF-8: E2 88 95 */
#define PN_FRACTION_FULLWIDTH_SOLIDUS 0xFF0Fu /* ／ FULLWIDTH SOLIDUS  UTF-8: EF BC 8F */
#define PN_FRACTION_SOLIDUS           0x002Fu /* / SOLIDUS            UTF-8: 2F       */

typedef struct pn_fraction {
    uint64_t numerator;   /* the top number                            */
    uint64_t denominator; /* the bottom number; 1 or more, never 0      */
    bool neg;             /* true for a negative value                  */
} pn_fraction;

/// @returns a `pn_fraction` in lowest terms with the
/// @param numerator and
/// @param denominator provided, which must not be 0, and the
/// @param neg sign provided; a numerator of 0 always gives 0/1, not negative
pn_fraction pn_fraction_make(uint64_t numerator, uint64_t denominator, bool neg);

/// Find the closest fraction to a double whose denominator is at
/// most a limit you choose:
///
///        pi    with max 10    ->   22/7      (Archimedes)
///        pi    with max 1000  ->   355/113   (Zu Chongzhi)
///        0.75  with max 1000  ->   3/4
///
/// It works through the value's continued fraction, which gives a
/// series of ever-closer fractions called convergents, and stops
/// when the next one would pass the limit. The double is converted
/// exactly, so the answer is the true closest fraction to the
/// double's value. Negating the value negates the answer. If two
/// fractions are exactly equally close, the one nearer zero wins:
/// 2.5 and -2.5 with max 1 give 2/1 and -2/1. Values of 2^64 or
/// more give UINT64_MAX/1.
/// https://en.wikipedia.org/wiki/Continued_fraction#Best_rational_approximations
///
/// @returns a `pn_fraction` in lowest terms close to the
/// @param value provided, which must be finite, with a denominator no
/// larger than the
/// @param max_denominator provided, which must be at least 1
pn_fraction pn_fraction_from_double(double value, uint64_t max_denominator);

/// Compares values, not fields: 1/3 equals 2/6 even in a struct
/// filled in by hand, and 0/1 equals -0/5.
///
/// @returns true when the
/// @param left and
/// @param right fractions provided are the same number; neither
/// denominator may be 0
bool pn_fraction_equals(pn_fraction left, pn_fraction right);

/// Divides numerator by denominator once, so the result is the
/// closest double to the fraction.
///
/// @returns the double nearest the
/// @param value provided, whose denominator must not be 0
double pn_fraction_to_double(pn_fraction value);

/// Read a fraction: an optional '-', digits, then optionally a slash
/// (⁄ U+2044, ∕ U+2215, ／ U+FF0F or /) and more digits. The result is
/// reduced to lowest terms:
///
///        "355⁄113"  -> 355/113
///        "-6∕8"     -> -3/4
///        "22／7"    -> 22/7
///        "3/1"      -> 3/1
///        "3"        -> 3/1
///
/// No spaces, no '+', and no empty side.
///
/// @returns a `pn_fraction` in lowest terms read from the
/// @param text provided, or a zeroed value and the
/// @param error encountered (check the error, since a zeroed value
///     is also a real fraction):
///     PN_ERROR_SYNTAX when the text doesn't follow the format,
///     PN_ERROR_RANGE when a number is bigger than UINT64_MAX,
///     PN_ERROR_DOMAIN when the denominator is 0.
pn_fraction pn_fraction_parse(const char* _Nonnull text, pn_error* _Nullable* _Nullable error);

#define PN_FRACTION_FORMAT_MAX_LEN (1 + 20 + 3 + 20 + 1) /* '-', top, ⁄, bottom, null */

/// @returns a new string, like "355⁄113", formatted from the
/// @param value provided, or NULL and the
/// @param error encountered; the caller owns the `char*` and should `free` it
[[nodiscard]]
char* _Nullable pn_fraction_format(pn_fraction value, pn_error* _Nullable* _Nullable error);

/// Write the value as "numerator⁄denominator" with ⁄ U+2044 FRACTION
/// SLASH, and '-' in front if negative: 355⁄113, -1⁄2. A denominator
/// of 1 writes just the whole number ("3"), so every output can be
/// read back by pn_fraction_parse. The text is null terminated and
/// needs at most PN_FRACTION_FORMAT_MAX_LEN bytes. On failure the
/// buffer is not changed.
///
/// @returns the caller's `char*` with the
/// @param value written into the
/// @param buffer provided, which holds the
/// @param buflen bytes provided; or NULL and the
/// @param error encountered:
///     PN_ERROR_DOMAIN when the denominator is 0,
///     PN_ERROR_BUFFER when buflen is too small.
char* _Nullable pn_fraction_format_into(pn_fraction value, char* _Nonnull buffer, size_t buflen,
                                        pn_error* _Nullable* _Nullable error);

// MARK: - Base 20

/*
    Base 20 (vigesimal): the number system of the Maya.

    A pn_base20 works like pn_base60 below, with base-20 digits
    (each 0 to 19) in place of base-60 ones:

    value = (-1)^neg x sum of places[i] x 20^(radix - 1 - i)

    places[0] is the biggest place. For a whole number, set radix to
    n_places. The radix point and the sign are modern additions; the
    Maya wrote neither.

    Converting to double is only a convenience: 0;5 (5/20 = 1/4) is
    exact in binary, but 0;1 (1/20) is not, because 20 = 4 x 5 has a
    factor that isn't 2. A double holds whole numbers exactly up to
    2^53, which covers 12 base-20 places (20^13 is about 8.2 x 10^16,
    past 2^53).

    Further reading:
    - https://en.wikipedia.org/wiki/Vigesimal
    - https://en.wikipedia.org/wiki/Maya_numerals
*/

#define PN_BASE20_MAX_PLACES 16 /* 20^16 is about 6.6 x 10^20; keeps the struct small */

typedef struct pn_base20 {
    uint8_t places[PN_BASE20_MAX_PLACES]; /* each 0..19, biggest place first */
    uint8_t n_places;                     /* how many places are used, 1..16 */
    uint8_t radix;                        /* places before the radix point; 0 = all fraction */
    bool neg;                             /* true for a negative value */
} pn_base20;

/// Read a base-20 value written in the modern notation: base-20
/// digits written in decimal, separated by ',' or ':', with at most
/// one ';' marking the radix point. (The Maya wrote none of this;
/// it is how scholars write their numbers today.)
///
///        "1,2,0,0"   -> places {1,2,0,0}, radix 4  (= 8800, a whole number)
///        "1;12,9"    -> places {1,12,9}, radix 1   (= 1 + 12/20 + 9/400)
///        "0;5"       -> places {0,5}, radix 1      (= 1/4)
///
/// No spaces, no empty places, no separator at the end. Watch out:
/// "399" is not the decimal number 399. It reads as one place worth
/// 399, which is too big (places go up to 19), so it is rejected.
/// Use pn_base20_from_u64 for ordinary whole numbers.
///
/// @param text - the text to read, null terminated
/// @param error - where to put the error report on failure:
///     PN_ERROR_SYNTAX when the text doesn't follow the format,
///     PN_ERROR_RANGE when a place is above 19 or has more than 2
///     digits, or there are more than PN_BASE20_MAX_PLACES places.
/// @returns the value read, or a zeroed value and the
/// @param error encountered.
pn_base20 pn_base20_parse(const char* _Nonnull text, pn_error* _Nullable* _Nullable error);

#define PN_BASE20_FORMAT_MAX_LEN (1 + 3 * PN_BASE20_MAX_PLACES + 1) /* '-', places, ',', null */

/// Write a value in the modern notation, the reverse of
/// pn_base20_parse. Places are separated by @param sep (',' or
/// ':'), and ';' marks the radix point when the value has both a
/// whole part and a fraction. The text is null terminated and needs
/// at most PN_BASE20_FORMAT_MAX_LEN bytes.
///
/// @param v - the value to write
/// @param sep - the separator between places
/// @param buf - where the text is written, with room for the null terminator
/// @param buflen - the size of buf in bytes
/// @param error - where to put the error report on failure:
///     PN_ERROR_SYNTAX when sep isn't ',' ':' or ';',
///     PN_ERROR_DOMAIN when v is malformed (no places or too many,
///     a place above 19, or radix past n_places),
///     PN_ERROR_BUFFER when buflen is too small. On failure buf is
///     not changed.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_base20_format(pn_base20 v, char sep, char* _Nonnull buf, size_t buflen,
                                 pn_error* _Nullable* _Nullable error);

/// Convert a whole number to base-20 places: divide by 20 again and
/// again, keeping the remainders. 0 becomes the single place {0}.
/// This can't fail: UINT64_MAX needs only 15 of the 16 places.
///
///        8800 -> places {1,2,0,0}   (1 x 8000 + 2 x 400 + 0 + 0)
///
/// @returns the base-20 value, a whole number (radix == n_places), of the
/// @param number provided
pn_base20 pn_base20_from_u64(uint64_t number);

/// Convert base-20 places back to a whole number with Horner's
/// method: start at 0, and for each place multiply by 20 and add the
/// place. https://en.wikipedia.org/wiki/Horner%27s_method
///
/// @returns the whole number held by the
/// @param value provided, or 0 and the
/// @param error encountered (check the error, since 0 is also a real
///     result):
///     PN_ERROR_DOMAIN when value is malformed or has a fraction part
///     (radix is not n_places),
///     PN_ERROR_RANGE when the value is too big for a uint64_t.
uint64_t pn_base20_to_u64(pn_base20 value, pn_error* _Nullable* _Nullable error);

/// Convert a base-20 value to a double. The whole part and the
/// fraction are each computed exactly as whole numbers where they
/// fit, then combined with one division, so the result is exact
/// whenever a double can hold the value at all.
///
/// @returns the double nearest the
/// @param value provided, which must be well-formed
double pn_base20_to_double(pn_base20 value);

/// Convert a double to base-20 places with exactly @param frac_places
/// fraction places, rounding the last one half away from zero (0.5
/// rounds up), the same way pn_base60_from_double does. Trailing
/// zero places are kept. Rounding can carry into the whole part:
/// 0.98 with one fraction place becomes 1;0.
///
/// @returns the base-20 value of the
/// @param x provided, with the
/// @param frac_places provided, or a zeroed value and the
/// @param error encountered:
///     PN_ERROR_DOMAIN for NaN, infinity, or frac_places above
///     PN_BASE20_MAX_PLACES,
///     PN_ERROR_RANGE when the result needs more than 16 places.
pn_base20 pn_base20_from_double(double x, uint8_t frac_places,
                                pn_error* _Nullable* _Nullable error);

// MARK: - Base 60

/*
    Base 60 (sexagesimal): the number system of Babylon.

    Babylonian mathematicians (from about 1800 BCE) counted in base
    60. We still do for time and angles: an hour has 60 minutes and
    a minute has 60 seconds. 60 divides evenly by 2, 3, 4, 5, 6, 10,
    12, 15, 20 and 30, so many fractions are exact: 1/3 is 0;20.

    A pn_base60 stores "places", base-60 digits from 0 to 59, with the
    biggest place first, just as 365 in base 10 is the places 3, 6, 5:

    value = (-1)^neg x sum of places[i] x 60^(radix - 1 - i)

    radix says how many places come before the radix point (the
    base-60 version of a decimal point). For a whole number, set
    radix to n_places.

    The Babylonians wrote no radix point and, for most of their
    history, no zero: a lone 𒁹 could mean 1, 60 or 1/60, and the
    reader worked out which from context. The radix and the sign
    here are modern additions so the type always means one number.

    The struct is about 20 bytes and passed by value, with no memory
    allocation. Converting to double is only a convenience: 0;20 is
    exactly 1/3, which binary can't write exactly, and past 60^8 a
    double can't hold every whole number.

    Further reading:
    - https://en.wikipedia.org/wiki/Sexagesimal
    - https://en.wikipedia.org/wiki/Babylonian_numerals
*/

#define PN_BASE60_MAX_PLACES 16 /* 60^16 is about 2.8 x 10^28; keeps the struct small */

typedef struct pn_base60 {
    uint8_t places[PN_BASE60_MAX_PLACES]; /* each 0..59, biggest place first */
    uint8_t n_places;                     /* how many places are used, 1..16 */
    uint8_t radix;                        /* places before the radix point; 0 = all fraction */
    bool neg;                             /* true for a negative value */
} pn_base60;

/*
    Scholars today write Babylonian numbers as decimal places
    separated by commas or colons. The tablet YBC 7289 shows the
    square root of 2 as 1,24,51,10, which is about 1.41421. Following
    Otto Neugebauer, a semicolon marks the radix point: 1;24,51,10
    means 1 + 24/60 + 51/3600 + 10/216000. No tablet ever wrote one.
    https://en.wikipedia.org/wiki/YBC_7289
*/

/// Read a base-60 value written in the modern notation: an optional
/// '-', then places of one or two decimal digits (each 0 to 59)
/// separated by ',' ':' or ';'. At most one ';' may appear, and it
/// marks the radix point:
///
///      "1,24,51,10"  -> places {1,24,51,10}, radix 4  (a whole number)
///      "1;24,51,10"  -> the same places, radix 1      (about 1.414)
///      "0;45"        -> places {0,45}, radix 1        (= 3/4)
///
/// No spaces, no empty places, no separator at the end. Watch out:
/// "3599" is not the decimal number 3599. It reads as one place of
/// 3599, which is too big (places go up to 59), so it is rejected.
/// Use pn_base60_from_u64 for ordinary whole numbers.
///
/// @returns the pn_base60 value read from the
/// @param text provided, or a zeroed value and the
/// @param error encountered (check the error, since a zeroed value
///     is also a real number):
///     PN_ERROR_SYNTAX when the text doesn't follow the format,
///     PN_ERROR_RANGE when a place is above 59 or has more than 2
///     digits, or there are more than PN_BASE60_MAX_PLACES places.
pn_base60 pn_base60_parse(const char* _Nonnull text, pn_error* _Nullable* _Nullable error);

/// @returns a new string formatted from the
/// @param value provided, using the
/// @param place_sep provided between places (see
/// pn_base60_format_into), or NULL and the
/// @param error encountered; the caller owns the `char*` and should `free` it
[[nodiscard]]
char* _Nullable pn_base60_format(pn_base60 value, char place_sep,
                                 pn_error* _Nullable* _Nullable error);

/// Write a value in the modern notation, the reverse of
/// pn_base60_parse. Places are separated by @param place_sep (','
/// ':' or ';'). Every place except the first is written with two
/// digits, as scholars do. When the value has both a whole part and
/// a fraction, ';' marks the radix point:
///
///       {1,24,51,10} radix 1, separator ','  ->  "1;24,51,10"
///       {2,23,3}     radix 3, separator ','  ->  "2,23,03"
///
/// With ';' as the separator and a radix point in the middle, the
/// text has two ';' and can't be read back, so use ',' or ':' when
/// you need to read it again. A negative value starts with '-'.
/// The text is null terminated and needs at most
/// PN_BASE60_FORMAT_MAX_LEN bytes. On failure the buffer is not
/// changed.
///
/// @returns the caller's `char*` with the
/// @param value written in, using the
/// @param place_sep between places, into the
/// @param buffer provided, which holds the
/// @param buflen bytes provided; or NULL and the
/// @param error encountered
#define PN_BASE60_FORMAT_MAX_LEN (1 + 3 * PN_BASE60_MAX_PLACES + 1) /* '-', places, ',', null */
char* _Nullable pn_base60_format_into(pn_base60 value, char place_sep, char* _Nonnull buffer,
                                      size_t buflen, pn_error* _Nullable* _Nullable error);

/// Convert a whole number to base-60 places: divide by 60 again and
/// again, keeping the remainders. This can't fail: UINT64_MAX needs
/// only 11 of the 16 places.
///
///        8583 -> places {2,23,3}, radix 3   (2 x 3600 + 23 x 60 + 3)
///        0    -> the single place {0}
///
/// @returns the base-60 value, a whole number (radix == n_places), of the
/// @param number provided
pn_base60 pn_base60_from_u64(uint64_t number);

/// Convert base-60 places back to a whole number with Horner's
/// method: start at 0, and for each place multiply by 60 and add the
/// place. The value must be a whole number (radix == n_places).
/// https://en.wikipedia.org/wiki/Horner%27s_method
///
/// @returns the whole number held by the
/// @param value provided, or 0 and the
/// @param error encountered (check the error, since 0 is also a real
///     result):
///     PN_ERROR_DOMAIN when value is malformed (including a place
///     above 59) or has a fraction part,
///     PN_ERROR_RANGE when the value is too big for a uint64_t.
uint64_t pn_base60_to_u64(pn_base60 value, pn_error* _Nullable* _Nullable error);

/// Convert a base-60 value to a double. The whole part and the
/// fraction are each computed exactly as whole numbers where they
/// fit, then combined with one division, so the result is exact
/// whenever a double can hold the value at all. Some values can't
/// be held exactly: 0;20 is 1/3, which repeats forever in binary,
/// and whole numbers past 2^53 (just under 60^9) get rounded.
///
/// @returns the double nearest the
/// @param value provided, which must be well-formed
double pn_base60_to_double(pn_base60 value);

/// Convert a double to base-60 places with exactly
/// @param frac_places fraction places (0 to PN_BASE60_MAX_PLACES),
/// rounding the last one half away from zero (0.5 rounds up), and
/// carrying to the left when a place reaches 60. Trailing zero
/// places are kept, so you always get the number of places you
/// asked for:
///
///        0.75 with 1 fraction place  ->  {45}     radix 0   (0;45 = 3/4)
///        1.5  with 1 fraction place  ->  {1,30}   radix 1   (1;30)
///
/// NaN and infinity are rejected. See pn_base60_to_double about
/// which values a double can hold exactly.
///
/// @returns the base-60 value of the
/// @param x provided, or a zeroed value and the
/// @param error encountered:
///     PN_ERROR_DOMAIN for NaN, infinity, or frac_places above the
///     maximum,
///     PN_ERROR_RANGE when the result needs more than
///     PN_BASE60_MAX_PLACES places.
pn_base60 pn_base60_from_double(double x, uint8_t frac_places,
                                pn_error* _Nullable* _Nullable error);

// MARK: - Cuneiform Numbers

/*
    Babylonian cuneiform numerals: base 60 pressed into clay.

    Scribes wrote every number with just two wedge shapes:

    𒁹  U+12079  CUNEIFORM SIGN DISH   vertical wedge  = 1    UTF-8: F0 92 81 B9
    𒌋  U+1230B  CUNEIFORM SIGN U      corner wedge    = 10   UTF-8: F0 92 8C 8B

    Each base-60 place (0 to 59) is some corner wedges for the tens,
    then some vertical wedges for the ones, and places are separated
    by one space (shown here as ␣):

    23    = 𒌋𒌋𒁹𒁹𒁹
    8583  = 𒁹𒁹␣𒌋𒌋𒁹𒁹𒁹␣𒁹𒁹𒁹     (places 2, 23, 3)
    3601  = 𒁹␣␣␣␣𒁹               (places 1, 0, 1: the empty place is a gap)

    Unicode has no Babylonian zero, and for most of their history
    the Babylonians didn't either: an empty place was just a gap.
    This module writes it as two extra spaces. That brings back the
    scribes' real problem: a lone 𒁹 could mean 1, 60 or 1/60, and
    only context told the reader which.

    Unicode also has a "Cuneiform Numbers and Punctuation" block
    (U+12400 and up), but those signs are for weights and measures,
    not the place-value digits used here.

    Further reading:
    - https://en.wikipedia.org/wiki/Babylonian_numerals
    - https://en.wikipedia.org/wiki/Cuneiform
*/

#define PN_CUNEIFORM_DISH 0x12079u /* 𒁹 vertical wedge = 1  */
#define PN_CUNEIFORM_U    0x1230Bu /* 𒌋 corner wedge    = 10 */

/// The number of bytes pn_babylonian_render writes for a
/// well-formed @param v, not counting the null terminator at the end. Never
/// fails; for a malformed value the answer means nothing, and
/// pn_babylonian_render reports the error.
size_t pn_babylonian_render_size(pn_base60 v);

/// Write @param v in cuneiform into @param buf as null terminated UTF-8.
/// Each place is its corner wedges then its vertical
/// wedges; places are one space apart; an empty place is the
/// two-space gap the scribes left. A negative value starts with '-'
/// (a modern addition). No radix point is drawn, since no tablet
/// had one, so fraction places look just like whole ones.
///
/// @param v - the value to write
/// @param buf - where the text is written, with room for the null terminator
/// @param buflen - the size of buf in bytes
/// @param error - where to put the error report on failure:
///     PN_ERROR_DOMAIN when v is malformed (no places or too many, a
///     place above 59, or radix past n_places),
///     PN_ERROR_BUFFER when buflen isn't bigger than
///     pn_babylonian_render_size(v). On failure buf is not changed.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_babylonian_render(pn_base60 v, char* _Nonnull buf, size_t buflen,
                                     pn_error* _Nullable* _Nullable error);

/// Read cuneiform text, the reverse of pn_babylonian_render.
///
/// @param text is UTF-8 made of
/// '-', the two wedge signs and spaces:
///
///          𒁹𒁹␣𒌋𒌋𒁹𒁹𒁹␣𒁹𒁹𒁹    ->    places {2,23,3}
///
/// (␣ marks a space.)  Each place is its corner wedges then its
/// vertical wedges. The spacing is strict, because it encodes the
/// empty places: between two groups of wedges there must be 1, 4,
/// 7, ... spaces (one separator plus two for each empty place);
/// before the first or after the last group, a multiple of 3; a
/// text of only spaces must be 2, 5, 8, ... long. Anything else is
/// rejected. So reading back what pn_babylonian_render wrote always
/// gives the same places.
///
/// Clay tablets show no radix point, so the result is read as a
/// whole number (radix == n_places). A lone 𒁹 reads as 1, even if
/// the scribe meant 60 or 1/60.
/// @param text - the cuneiform text to read, null terminated
/// @param error - where to put the error report on failure:
///     PN_ERROR_SYNTAX for any other byte, wrong spacing, an empty
///     string, or a vertical wedge before a corner wedge in one place,
///     PN_ERROR_RANGE for a place with more than 5 corner wedges or 9
///     vertical wedges, or more than PN_BASE60_MAX_PLACES places.
/// @returns the value read, or a zeroed value and the
/// @param error encountered.
pn_base60 pn_babylonian_parse(const char* _Nonnull text, pn_error* _Nullable* _Nullable error);

// MARK: - Egyptian Numerals

/*
    Egyptian hieroglyphic numerals.

    Ancient Egypt (from about 3000 BCE) wrote numbers with one sign
    for each power of ten, repeated as many times as needed. It is
    decimal, but additive, not positional: the signs look different,
    so their order doesn't change the value. All seven are in the
    Egyptian Hieroglyphs block of Unicode, 4 bytes each in UTF-8:

    𓏤  U+133E4  EGYPTIAN HIEROGLYPH Z001  single stroke   = 1
    𓎆  U+13386  EGYPTIAN HIEROGLYPH V020  cattle hobble   = 10
    𓍢  U+13362  EGYPTIAN HIEROGLYPH V001  coil of rope    = 100
    𓆼  U+131BC  EGYPTIAN HIEROGLYPH M012  lotus           = 1 000
    𓂭  U+130AD  EGYPTIAN HIEROGLYPH D050  bent finger     = 10 000
    𓆐  U+13190  EGYPTIAN HIEROGLYPH I008  tadpole         = 100 000
    𓁨  U+13068  EGYPTIAN HIEROGLYPH C011  Heh, a god with
                                           raised arms     = 1 000 000

    There is no sign above the Heh and no zero; a scribe simply left
    a blank. So 0 is the empty string, and the largest value is
    9 999 999 (nine of every sign). Ten Heh signs would be
    10 000 000, which this module doesn't write.

    This module writes the biggest signs first, each sign in one
    group, with no spaces:

    4622  = 𓆼𓆼𓆼𓆼𓍢𓍢𓍢𓍢𓍢𓍢𓎆𓎆𓏤𓏤   (4 lotus, 6 coils, 2 hobbles, 2 strokes)

    Further reading:
    - https://en.wikipedia.org/wiki/Egyptian_numerals
*/

#define PN_EGYPTIAN_STROKE    0x133E4u   /* 𓏤 single stroke   = 1          */
#define PN_EGYPTIAN_HOBBLE    0x13386u   /* 𓎆 cattle hobble   = 10         */
#define PN_EGYPTIAN_COIL      0x13362u   /* 𓍢 coil of rope    = 100        */
#define PN_EGYPTIAN_LOTUS     0x131BCu   /* 𓆼 lotus           = 1 000      */
#define PN_EGYPTIAN_FINGER    0x130ADu   /* 𓂭 bent finger     = 10 000     */
#define PN_EGYPTIAN_TADPOLE   0x13190u   /* 𓆐 tadpole         = 100 000    */
#define PN_EGYPTIAN_HEH       0x13068u   /* 𓁨 Heh god         = 1 000 000  */
#define PN_EGYPTIAN_MAX_VALUE 9999999ull /* nine of each sign */

/// The number of bytes pn_egyptian_render writes for @param n, not
/// counting the null terminator: 4 per sign, which is 4 times the sum of n's
/// decimal digits. Never fails; above PN_EGYPTIAN_MAX_VALUE the
/// answer means nothing, and pn_egyptian_render reports
/// PN_ERROR_RANGE. 0 gives 0, the empty string.
size_t pn_egyptian_render_size(uint64_t n);

/// Write @param n in hieroglyphs into @param buf as null terminated UTF-8.
/// Each decimal digit d becomes d copies of its
/// power's sign, biggest power first, with no spaces:
/// 4622 = 𓆼𓆼𓆼𓆼𓍢𓍢𓍢𓍢𓍢𓍢𓎆𓎆𓏤𓏤.  0 is the empty string. The buffer
/// needs pn_egyptian_render_size(n) bytes plus the null terminator.
///
/// @param buf - where the text is written; must not be NULL
/// @param buflen - the size of buf in bytes
/// @param error - where to put the error report on failure:
///     PN_ERROR_RANGE when n is above PN_EGYPTIAN_MAX_VALUE,
///     PN_ERROR_BUFFER when buflen is too small. On failure buf is
///     not changed.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_egyptian_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                                   pn_error* _Nullable* _Nullable error);

/// Read hieroglyphs, the reverse of pn_egyptian_render. Only the
/// standard form is accepted: one group per sign, groups from
/// biggest to smallest, 1 to 9 signs in each. The value is the sum
/// of the signs, so reading back what was written always gives the
/// same number. An empty string reads as 0.
///
/// @param text - the text to read, null terminated
/// @param error - where to put the error report on failure:
///     PN_ERROR_SYNTAX for a byte that isn't a sign, a sign cut off
///     short, groups out of order, or one sign in two groups,
///     PN_ERROR_RANGE for ten or more of one sign (a scribe would
///     have written one sign of the next power instead).
/// @returns the number read, or 0 and the
/// @param error encountered (check the error, since 0 is also a
///     real result).
uint64_t pn_egyptian_parse(const char* _Nonnull text, pn_error* _Nullable* _Nullable error);

// MARK: - Hanzi Numerals

/*
    Chinese numerals (hanzi).

    Chinese numerals are decimal, but they don't use position alone
    to show a digit's value. Each digit is followed by a multiplier
    sign: 千 (thousand), 百 (hundred), 十 (ten); the ones digit has no
    multiplier. Numbers of 10 000 and up are split around 万 (ten
    thousand): the value is A x 10 000 + B, each part written the
    same way. All signs are 3 bytes in UTF-8:

    一 U+4E00 = 1     二 U+4E8C = 2     三 U+4E09 = 3
    四 U+56DB = 4     五 U+4E94 = 5     六 U+516D = 6
    七 U+4E03 = 7     八 U+516B = 8     九 U+4E5D = 9
    十 U+5341 = 10    百 U+767E = 100   千 U+5343 = 1000
    万 U+4E07 = 10 000

    四千六百二十二                   = 4622
    九千九百九十九万九千九百九十九    = 99 999 999, the largest value here

    A zero digit is left out. The multipliers already say where
    every digit belongs, so nothing is lost, but this module puts one
    space (shown as ␣) where digits were skipped, so a reader can see
    the gap: 1005 = 一千␣五, 1050 = 一千␣五十, 50 000 005 = 五千万␣五.
    Modern Chinese writes the sign 零 (zero) there instead: 一千零五.

    This module uses the full formal form, 一十四 for 14, not the
    everyday 十四.  Signs above 万, such as 亿 (10^8), are not
    included, so PN_HANZI_MAX_VALUE is 99 999 999.

    Further reading:
    - https://en.wikipedia.org/wiki/Chinese_numerals
*/

#define PN_HANZI_MAX_VALUE 99999999ull /* 九 in every place of both sections */

/// The number of bytes pn_hanzi_render writes for @param n, not
/// counting the null terminator: 3 per sign (digits, multipliers and 万) plus
/// 1 per space. Never fails; above PN_HANZI_MAX_VALUE the answer
/// means nothing, and pn_hanzi_render reports the error. 0 gives 0.
///
/// @param n - the number to measure
/// @returns the number of bytes pn_hanzi_render would write.
size_t pn_hanzi_render_size(uint64_t n);

/// Write @param n (0 to PN_HANZI_MAX_VALUE) in Chinese numerals into
/// @param buf as UTF-8, null terminated: 4622 = 四千六百二十二,
/// 1005 = 一千␣五.  Skipped digits are marked by one space. 0 is the
/// empty string.
/// @param n - the number to write
/// @param buf - where the text is written, with room for the null terminator
/// @param buflen - the size of buf in bytes
/// @param error - where to put the error report on failure:
///     PN_ERROR_RANGE when n is above PN_HANZI_MAX_VALUE,
///     PN_ERROR_BUFFER when buflen isn't bigger than
///     pn_hanzi_render_size(n). On failure buf is not changed.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_hanzi_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                                pn_error* _Nullable* _Nullable error);

/// Read Chinese numerals, the reverse of pn_hanzi_render. Only the
/// standard form is accepted: terms (a digit with 千, 百 or 十, or a
/// lone ones digit) going from bigger places to smaller, at most one
/// 万 right after the top section, and one space between two terms
/// exactly when digits were skipped between them. An empty string
/// reads as 0. Rejected with PN_ERROR_SYNTAX: other bytes, a sign
/// cut off short, 十四 without its 一, a space where nothing was
/// skipped, and a skip without a space.
///
/// @param text - the text to read, null terminated
/// @param error - where to put the error report on failure.
/// @returns the number read, or 0 and the
/// @param error encountered (check the error, since 0 is also a
///     real result).
uint64_t pn_hanzi_parse(const char* _Nonnull text, pn_error* _Nullable* _Nullable error);

/*
    Counting rod numerals.

    Counting rods (算筹) were small sticks laid out on a counting
    board to do arithmetic, used in China from at least the Warring
    States period (475 to 221 BCE) and later in Japan, Korea and
    Vietnam. They were true place-value decimal: one column per
    place. Each digit has two forms, one the other turned sideways,
    and the columns alternate between them so a reader can tell
    neighbouring columns apart. All are 4 bytes in UTF-8:

    U+1D360..U+1D368  COUNTING ROD UNIT DIGIT ONE..NINE   𝍠 𝍡 𝍢 𝍣 𝍤 𝍥 𝍦 𝍧 𝍨
    U+1D369..U+1D371  COUNTING ROD TENS DIGIT ONE..NINE   𝍩 𝍪 𝍫 𝍬 𝍭 𝍮 𝍯 𝍰 𝍱

    The ones column uses the unit set, the tens column the tens set,
    the hundreds the unit set again, and so on.

    The rods had no zero: an empty column was just left empty. This
    module writes one space (shown as ␣) between neighbouring
    columns, and an empty column writes nothing, so there are always
    (number of columns - 1) spaces:

    𝍪␣𝍢    = 23
    𝍠␣␣𝍠   = 101   (empty tens column)
    𝍠␣␣    = 100   (empty tens and ones columns)
    𝍩␣     = 10    (empty ones column)

    A digit must use the right form for its column: writing 11 as
    𝍠␣𝍠 instead of 𝍩␣𝍠 is rejected. Negative numbers (red and black
    rods) and the modern zero 〇 (U+3007) are not used.

    Further reading:
    - https://en.wikipedia.org/wiki/Counting_rods
*/

// MARK: - Rod Numerals

#define PN_ROD_UNIT_DIGIT_ONE 0x1D360u               /* 𝍠; digit d is this + d - 1 */
#define PN_ROD_TENS_DIGIT_ONE 0x1D369u               /* 𝍩; digit d is this + d - 1 */
#define PN_ROD_MAX_PLACES     19                     /* 10^19 - 1 fits in a uint64_t */
#define PN_ROD_MAX_VALUE      9999999999999999999ull /* 10^19 - 1: nineteen nines */

/// The number of bytes pn_rods_render writes for @param n, not
/// counting the null terminator: 4 per rod plus one space between
/// neighbouring columns, which is 4 x (nonzero digits) + (digits -
/// 1). Never fails; above PN_ROD_MAX_VALUE the answer means
/// nothing, and pn_rods_render reports the error.
size_t pn_rods_render_size(uint64_t n);

/// Write @param n in counting rods into @param buf as null terminated UTF-8:
/// one rod sign per nonzero digit, biggest column
/// first, one space between neighbouring columns (so 101 = 𝍠␣␣𝍠).
/// 0 is the empty string. The buffer needs
/// pn_rods_render_size(@param n) bytes plus the null terminator.
///
/// @param n - the number to write
/// @param buf - where the text is written, with room for the null terminator
/// @param buflen - the size of buf in bytes
/// @param error - where to put the error report on failure:
///     PN_ERROR_RANGE when n is above PN_ROD_MAX_VALUE,
///     PN_ERROR_BUFFER when buflen is too small. On failure buf is
///     not changed.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_rods_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                               pn_error* _Nullable* _Nullable error);

/// Read counting rods, the reverse of pn_rods_render. The number of
/// columns is the number of spaces + 1; the first rod is in the top
/// column, and each space moves down one column:
///
///     𝍪␣𝍢    ->  23
///     𝍠␣␣𝍠   ->  101   (empty tens column)
///     𝍩␣     ->  10    (empty ones column)
///
/// Reading back what pn_rods_render wrote always gives the same
/// number. An empty string reads as 0.
///
/// @param text - the text to read, null terminated
/// @param error - where to put the error report on failure:
///     PN_ERROR_SYNTAX for a leading space, only spaces, two rods
///     with no space between them, a rod in the wrong form for its
///     column, any other byte, or a sign cut off short,
///     PN_ERROR_RANGE for more than PN_ROD_MAX_PLACES columns.
/// @returns the number read, or 0 and the
/// @param error encountered (check the error, since 0 is also a
///     real result).
uint64_t pn_rods_parse(const char* _Nonnull text, pn_error* _Nullable* _Nullable error);

/*
    Brahmi numerals.

    Brahmi numerals were used in India from about the 3rd century
    BCE, and today's digits 0 to 9 descend from their shapes. But
    the system itself was different: not positional, and with no
    zero. Every value 1 to 9 and every ten 10 to 90 has its own sign,
    so 47 is two signs (𑁞𑁘, forty and seven), not eleven marks as in
    Egyptian. All are 4 bytes in UTF-8:

    U+11052..U+1105A  BRAHMI NUMBER ONE..NINE      𑁒 𑁓 𑁔 𑁕 𑁖 𑁗 𑁘 𑁙 𑁚
    U+1105B..U+11063  BRAHMI NUMBER TEN..NINETY    𑁛 𑁜 𑁝 𑁞 𑁟 𑁠 𑁡 𑁢 𑁣
    U+11064           BRAHMI NUMBER ONE HUNDRED    𑁤
    U+11065           BRAHMI NUMBER ONE THOUSAND   𑁥
    U+1107F           BRAHMI NUMBER JOINER         𑁿

    Hundreds and thousands use multiplication: the power sign, the
    joiner, then a digit 2 to 9. The joiner is what says "times":

    400   = 𑁤𑁿𑁕       hundred x four
    104   = 𑁤𑁕         hundred + four (no joiner)
    100   = 𑁤          the hundred sign alone

    A number is up to four terms, thousands then hundreds then tens
    then ones, each used at most once, with no spaces:

    256   = 𑁤𑁿𑁓𑁟𑁗       two hundred, fifty, six
    153   = 𑁤𑁟𑁔         hundred, fifty, three
    9999  = 𑁥𑁿𑁚𑁤𑁿𑁚𑁣𑁚     nine thousand, nine hundred, ninety, nine

    𑁚 appears three times in 9999; that is fine because each one is
    in a different term. There is no sign above one thousand and no
    zero, so 0 is the empty string and the largest value is 9999.

    Further reading:
    - https://en.wikipedia.org/wiki/Brahmi_numerals
    - https://www.unicode.org/charts/PDF/U11000.pdf (the Unicode chart)
*/

// MARK: - Brahmi Numerals

#define PN_BRAHMI_NUMBER_ONE      0x11052u /* 𑁒 through U+1105A 𑁚 = 1..9      */
#define PN_BRAHMI_NUMBER_TEN      0x1105Bu /* 𑁛 through U+11063 𑁣 = 10..90    */
#define PN_BRAHMI_NUMBER_HUNDRED  0x11064u /* 𑁤 one hundred                 */
#define PN_BRAHMI_NUMBER_THOUSAND 0x11065u /* 𑁥 one thousand                */
#define PN_BRAHMI_NUMBER_JOINER   0x1107Fu /* 𑁿 the joiner: means "times"    */
#define PN_BRAHMI_MAX_VALUE       9999u    /* the largest: no sign above 𑁥 */

/// The number of bytes pn_brahmi_render writes for @param n, not
/// counting the null terminator: 4 per sign, joiner included. A hundreds or
/// thousands term is 4 bytes for a 1 (the power sign alone) or 12
/// for 2 to 9 (power, joiner, digit); a tens or ones term is 4 bytes.
/// Never fails; above PN_BRAHMI_MAX_VALUE the answer means nothing,
/// and pn_brahmi_render reports the error.
size_t pn_brahmi_render_size(uint64_t n);

/// Write @param n in Brahmi numerals into @param buf as UTF-8,
/// null terminated: thousands, hundreds, tens, ones, as
/// described above (256 = 𑁤𑁿𑁓𑁟𑁗). 0 is the empty string. The
/// buffer needs pn_brahmi_render_size(@param n) bytes plus the 0
/// byte. On failure buf is not changed.
///
/// @param n - the number to write
/// @param buf - where the text is written, with room for the null terminator
/// @param buflen - the size of buf in bytes
/// @param error - where to put the error report on failure:
///     PN_ERROR_RANGE when n is above PN_BRAHMI_MAX_VALUE,
///     PN_ERROR_BUFFER when buflen is too small.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_brahmi_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                                 pn_error* _Nullable* _Nullable error);

/// Read Brahmi numerals, the reverse of pn_brahmi_render. Only the
/// standard form is accepted: at most four terms (thousands,
/// hundreds, tens, ones), each at most once, in that order:
///
///     𑁤𑁿𑁓𑁟𑁗  ->  256    (hundred x two, fifty, six)
///     𑁤𑁕      ->  104    (hundred + four)
///     𑁥𑁒      ->  1001   (thousand + one)
///
/// Reading back what pn_brahmi_render wrote always gives the same
/// number. An empty string reads as 0.
///
/// @param text - the text to read, null terminated
/// @param error - where to put the error report on failure:
///     PN_ERROR_SYNTAX for a term used twice, terms out of order, a
///     joiner anywhere except between a power sign and a digit 2 to
///     9, a joined 1 (100 is just 𑁤), 𑁤𑁕𑁕 (the ones term twice), any
///     other byte, or a sign cut off short.
/// @returns the number read, or 0 and the
/// @param error encountered (check the error, since 0 is also a
///     real result).
uint64_t pn_brahmi_parse(const char* _Nonnull text, pn_error* _Nullable* _Nullable error);

/*
    Maya numerals: base 20 in bars and dots.

    The Maya wrote each base-20 place as dots (ones) and bars (fives),
    and they had a true zero, usually drawn as a shell. Unicode has
    one ready-made sign for each place value, 4 bytes each in UTF-8:

    U+1D2E0..U+1D2E4  MAYAN NUMERAL ZERO..FOUR         𝋠 𝋡 𝋢 𝋣 𝋤   shell, dots
    U+1D2E5..U+1D2E9  MAYAN NUMERAL FIVE..NINE         𝋥 𝋦 𝋧 𝋨 𝋩   one bar
    U+1D2EA..U+1D2EE  MAYAN NUMERAL TEN..FOURTEEN      𝋪 𝋫 𝋬 𝋭 𝋮   two bars
    U+1D2EF..U+1D2F3  MAYAN NUMERAL FIFTEEN..NINETEEN  𝋯 𝋰 𝋱 𝋲 𝋳   three bars

    The Maya stacked places vertically with the smallest at the
    bottom. This module writes them left to right, biggest first,
    with one space (shown as ␣) between places:

    𝋡␣𝋣        = 23     (1 x 20 + 3)
    𝋡␣𝋭        = 33     (1 x 20 + 13)
    𝋡␣𝋡␣𝋩     = 429    (1 x 400 + 1 x 20 + 9)
    𝋡␣𝋢␣𝋠␣𝋠  = 8800   (1 x 8000 + 2 x 400; the shells are the empty places)
    𝋠          = 0

    Because every place, even an empty one, has a sign, there is
    never any doubt about how many places a number has, unlike the
    Babylonian gap. The radix point and '-' are modern additions,
    and pn_mayan_parse always reads a whole number (radix ==
    n_places).

    Further reading:
    - https://en.wikipedia.org/wiki/Maya_numerals
    - https://www.unicode.org/charts/PDF/U1D2E0.pdf (the Unicode chart)
*/

// MARK: - Mayan Numerals

#define PN_MAYAN_DIGIT_ZERO 0x1D2E0u /* 𝋠 zero; digit d is this + d */

/// The number of bytes pn_mayan_render writes for @param v, not
/// counting the null terminator: 4 per place, one space between places, and
/// 1 for '-' if negative. Never fails; for a malformed value the
/// answer means nothing, and pn_mayan_render reports the error.
size_t pn_mayan_render_size(pn_base20 v);

/// Write @param v in Maya numerals into @param buf as null terminated UTF-8:
/// one sign per place, one space between places, so
/// 429 = 𝋡␣𝋡␣𝋩.  An empty place writes the shell 𝋠, not a gap.
///
/// @param v - the value to write
/// @param buf - where the text is written, with room for the null terminator
/// @param buflen - the size of buf in bytes
/// @param error - where to put the error report on failure:
///     PN_ERROR_DOMAIN when v is malformed (no places or too many, a
///     place above 19, or radix past n_places),
///     PN_ERROR_BUFFER when buflen isn't bigger than
///     pn_mayan_render_size(v). On failure buf is not changed.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_mayan_render(pn_base20 v, char* _Nonnull buf, size_t buflen,
                                pn_error* _Nullable* _Nullable error);

/// Read Maya numerals, the reverse of pn_mayan_render. @param text
/// is UTF-8: an optional '-', then signs
/// separated by single spaces, like 𝋡␣𝋡␣𝋩.  Reading back what
/// pn_mayan_render wrote always gives the same places, read as a
/// whole number (radix == n_places), since the signs show no radix
/// point.
///
/// An empty string is an error, not zero: zero is written 𝋠.
///
/// @param text - the text to read, null terminated
/// @param error - where to put the error report on failure:
///     PN_ERROR_SYNTAX for a space at the start or end, two spaces in
///     a row, two signs with no space between them, any other byte
///     (including the unassigned U+1D2F4..U+1D2FF), a sign cut off
///     short, or an empty string,
///     PN_ERROR_RANGE for more than PN_BASE20_MAX_PLACES places.
/// @returns the value read, or a zeroed value and the
/// @param error encountered.
pn_base20 pn_mayan_parse(const char* _Nonnull text, pn_error* _Nullable* _Nullable error);

/*
    Inca quipu numerals: decimal numbers tied in knots.

    The Inca kept records on quipus: a main cord with many cords
    hanging from it, each holding a number in clusters of knots. The
    clusters are decimal places, with the ones place nearest the loose
    end and higher places closer to the main cord. Leland Locke
    showed this in 1912. Each cord holds one number; the colour and
    twist of the cords carried other meanings not handled here.

    Unicode has no quipu signs, so this module borrows two common
    ones, 3 bytes each in UTF-8:

    •  U+2022  BULLET    a simple knot; d of them write the digit d in
                         any place above the ones
    ∞  U+221E  INFINITY  in the ones place: one is a figure-eight
                         knot (1), and d of them are a "long knot"
                         with d turns (2 to 9)

    Places are written biggest first with one space (shown as ␣)
    between neighbouring places. An empty place is just no knots, so
    there are always (number of places - 1) spaces:

    •••••••␣•••␣∞          = 731
    ••••••••␣␣∞∞∞∞         = 804    (empty tens place)
    •␣••••␣•••••••••␣∞∞∞   = 1493
    •␣                     = 10     (empty ones place)
    ∞                      = 1
    (empty string)         = 0      (no knots at all)

    The knot type also marks the ones place: ∞ only appears there, and
    • never does, so a reader can always find where the number ends.

    Further reading:
    - https://en.wikipedia.org/wiki/Quipu
    - Locke, The Ancient Quipu or Peruvian Knot Record (1912)
    - Ascher and Ascher, Mathematics of the Incas: Code of the Quipu (1997)
*/

// MARK: - Quipu Numerals

#define PN_QUIPU_SIMPLE_KNOT 0x2022u                /* • BULLET   UTF-8: E2 80 A2         */
#define PN_QUIPU_LONG_KNOT   0x221Eu                /* ∞ INFINITY UTF-8: E2 88 9E         */
#define PN_QUIPU_MAX_PLACES  19                     /* 10^19 - 1 fits in a uint64_t      */
#define PN_QUIPU_MAX_VALUE   9999999999999999999ull /* nineteen nines               */

/// The number of bytes pn_quipu_render writes for @param n, not
/// counting the null terminator. Never fails; above PN_QUIPU_MAX_VALUE the
/// answer means nothing, and pn_quipu_render reports the error.
size_t pn_quipu_render_size(uint64_t n);

/// Write @param n (0 to PN_QUIPU_MAX_VALUE) as quipu knots into
/// @param buf as UTF-8, null terminated: d bullets for a digit
/// d above the ones place, d infinity signs for the ones digit, one
/// space between places (731 = •••••••␣•••␣∞). An empty place
/// writes no knots, and 0 is the empty string.
/// @param n - the number to write
/// @param buf - where the text is written, with room for the null terminator
/// @param buflen - the size of buf in bytes
/// @param error - where to put the error report on failure:
///     PN_ERROR_RANGE when n is above PN_QUIPU_MAX_VALUE,
///     PN_ERROR_BUFFER when buflen isn't bigger than
///     pn_quipu_render_size(n). On failure buf is not changed.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_quipu_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                                pn_error* _Nullable* _Nullable error);

/// Read quipu knots, the reverse of pn_quipu_render. The number of
/// places is the number of spaces + 1; the first cluster is in the
/// top place, and each space moves down one place. Each cluster is
/// 1 to 9 of the same knot: bullets above the ones place, infinity
/// signs in the ones place only. Reading back what pn_quipu_render
/// wrote always gives the same number. An empty string reads as 0.
///
/// @param text - the text to read, null terminated
/// @param error - where to put the error report on failure:
///     PN_ERROR_SYNTAX for a bullet in the ones place, an infinity
///     sign above it, mixed knots in one cluster, a space at the
///     start, clusters with no space between them, any other byte
///     (including '-'), or a sign cut off short,
///     PN_ERROR_RANGE for ten or more knots in one cluster, or more
///     than PN_QUIPU_MAX_PLACES places.
/// @returns the number read, or 0 and the
/// @param error encountered (check the error, since 0 is also a
///     real result).
uint64_t pn_quipu_parse(const char* _Nonnull text, pn_error* _Nullable* _Nullable error);

/*
    Greek alphabetic numerals.

    The ancient Greeks used their alphabet as digits. Twenty-seven
    letters stand for 1 to 9, 10 to 90 and 100 to 900. Three of them
    are old letters kept only for numbers: ϛ stigma (6), ϟ koppa (90)
    and ϡ sampi (900).

    ones:      α β γ δ ε ϛ ζ η θ    = 1 to 9
    tens:      ι κ λ μ ν ξ ο π ϟ    = 10 to 90
    hundreds:  ρ σ τ υ φ χ ψ ω ϡ    = 100 to 900

    A number below 1000 is at most one hundreds letter, then one tens
    letter, then one ones letter: 256 = σνϛ (200 + 50 + 6). Each value
    has exactly one spelling. The keraia ʹ (U+0374) at the end marks
    the letters as a number, not a word. Thousands use the same
    letters with the lower keraia ͵ (U+0375) in front. There is no
    zero.

    Written with nothing between them, the thousands and the rest can
    run together: ͵ια could mean 11 000 (ια thousands) or 10 001 (ι
    thousands and α). This module puts one space (shown as ␣) between
    the two groups when both are present:

    αʹ            = 1
    ιθʹ           = 19
    σνϛʹ          = 256
    χϛʹ           = 606
    ϡϟθʹ          = 999
    ͵αʹ           = 1000
    ͵α␣αʹ         = 1001
    ͵β␣ιθʹ        = 2019
    ͵ι␣αʹ         = 10 001
    ͵ιαʹ          = 11 000
    ͵ϡϟθ␣ϡϟθʹ     = 999 999, the largest value here

    Only lowercase letters are used. Capitals, the final sigma ς, the
    lunate sigma ϲ and any other character are errors. 0 is the empty
    string.

    One trap if you normalize Unicode text: the keraia U+0374 changes
    to U+02B9 (MODIFIER LETTER PRIME) under NFC and NFD normalization
    and never changes back, and this module does not accept U+02B9.
    https://en.wikipedia.org/wiki/Unicode_equivalence

    Further reading:
    - https://en.wikipedia.org/wiki/Greek_numerals
    - https://en.wikipedia.org/wiki/Keraia
*/

// MARK: - Greek Numerals

#define PN_GREEK_MAX_VALUE 999999ull /* ͵ϡϟθ␣ϡϟθʹ */

/// The number of bytes pn_greek_render writes for @param n, not
/// counting the null terminator: 2 per sign, keraia included, plus 1 for the
/// space between the two groups. 0 gives 0. Never fails; outside
/// 1..PN_GREEK_MAX_VALUE pn_greek_render reports the error.
size_t pn_greek_render_size(uint64_t n);

/// Write @param n (0 to PN_GREEK_MAX_VALUE) in Greek numerals into
/// @param buf as UTF-8, null terminated: 2019 = ͵β␣ιθʹ.  0 is
/// the empty string.
/// @param n - the number to write
/// @param buf - where the text is written, with room for the null terminator
/// @param buflen - the size of buf in bytes
/// @param error - where to put the error report on failure:
///     PN_ERROR_RANGE when n is above PN_GREEK_MAX_VALUE,
///     PN_ERROR_BUFFER when buflen can't hold
///     pn_greek_render_size(n) plus the null terminator. On failure buf is
///     not changed.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_greek_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                                pn_error* _Nullable* _Nullable error);

/// Read Greek numerals, the reverse of pn_greek_render. Only the
/// standard form is accepted: ʹ at the very end and nowhere else, ͵
/// at the very start at most once, at most one space (only after ͵,
/// never first or last), and in each group at most one hundreds, one
/// tens and one ones letter, in that order. An empty string reads as
/// 0.
///
/// @param text - the text to read, null terminated
/// @param error - where to put the error report on failure:
///     PN_ERROR_SYNTAX for anything else, including capitals, other
///     sigma forms, accent marks, U+02B9, any ASCII byte, or badly
///     formed UTF-8.
/// @returns the number read, or 0 and the
/// @param error encountered (check the error, since 0 is also a
///     real result).
uint64_t pn_greek_parse(const char* _Nonnull text, pn_error* _Nullable* _Nullable error);

/*
    Roman numerals.

    Romans wrote numbers with seven letters: I (1), V (5), X (10),
    L (50), C (100), D (500), M (1000). Values are added up, except
    that a smaller letter before a bigger one is subtracted: IV is
    5 - 1 = 4 and IX is 10 - 1 = 9. There is no zero; around 725 CE
    Bede or his colleagues wrote the word nulla ("nothing") instead.

    This module uses the standard modern form: each decimal digit is
    written with a fixed pattern, thousands first, using the six
    subtractive pairs IV, IX, XL, XC, CD and CM. So each number has
    exactly one spelling. Clock-face IIII and shortcuts like IC for 99
    are not accepted.

    I                 = 1
    IV                = 4
    IX                = 9
    XIV               = 14
    XLIV              = 44
    XC                = 90
    CD                = 400
    CM                = 900
    MCMXCIX           = 1999
    MMMDCCCLXXXVIII   = 3888, the longest (15 letters)
    MMMCMXCIX         = 3999, the largest

    Bigger numbers were written with a bar over the letters, which
    this module does not support. 0 is the empty string. Only
    capital ASCII letters are accepted.

    Further reading:
    - https://en.wikipedia.org/wiki/Roman_numerals
*/

// MARK: - Roman Numerals

#define PN_ROMAN_MAX_VALUE 3999ull /* MMMCMXCIX */

#define PN_ROMAN_FORMAT_MAX_LEN 16 /* MMMDCCCLXXXVIII is 15 letters, plus the null terminator */

/// The number of letters pn_roman_render writes for @param n, not
/// counting the null terminator. Returns 0 outside 1..PN_ROMAN_MAX_VALUE.
size_t pn_roman_render_size(uint64_t n);

/// Write @param n (0 to PN_ROMAN_MAX_VALUE) in Roman numerals into
/// @param buf, null terminated: 1999 = MCMXCIX. 0 is the empty
/// string.
/// @param n - the number to write
/// @param buf - where the text is written, with room for the null terminator
/// @param buflen - the size of buf in bytes
/// @param error - where to put the error report on failure:
///     PN_ERROR_RANGE when n is above PN_ROMAN_MAX_VALUE,
///     PN_ERROR_BUFFER when buflen can't hold
///     pn_roman_render_size(n) plus the null terminator. On failure buf is
///     not changed.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_roman_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                                pn_error* _Nullable* _Nullable error);

/// Read Roman numerals, the reverse of pn_roman_render. The reader
/// adds up the letters, then writes the sum back out and checks that
/// it matches the input exactly, so only the standard form is
/// accepted. An empty string reads as 0.
///
/// @param text - the text to read, null terminated
/// @param error - where to put the error report on failure:
///     PN_ERROR_SYNTAX for non-standard spellings (IIII, IL, VX, XCX),
///     lowercase letters, spaces, the null terminator, or any non-ASCII byte.
/// @returns the number read, or 0 and the
/// @param error encountered (check the error, since 0 is also a
///     real result).
uint64_t pn_roman_parse(const char* _Nonnull text, pn_error* _Nullable* _Nullable error);

// MARK: - Math

/*
    Early number theory: the math under the numerals.

    These functions come from the oldest written number theory:
    Euclid's Elements (Books VII to IX, about 300 BCE), the sieve of
    Eratosthenes, and the Pythagorean study of square numbers and
    right triangles. They answer questions such as "what is the
    largest number that divides both of these?", "is this number
    prime?" and "what primes multiply to make this number?".

    Every function works on whole numbers that fit in a uint64_t,
    from 0 up to 2^64 - 1 (about 1.8 x 10^19).

    How the prime test works. A prime is a whole number above 1
    whose only divisors are 1 and itself. pn_math_is_prime gives an
    exact yes or no, never a "probably", and it uses two methods:

    - Below 2^32 it uses trial division: try dividing by small odd
      numbers and see if any divides evenly. If a number n is
      composite (not prime), at least one of its factors is no bigger
      than the square root of n, so you only need to try divisors up
      to that square root. For n below 2^32 the square root is below
      2^16 = 65536, so the search is short.

    - From 2^32 up, trial division would take billions of steps, so
      it uses the Miller-Rabin test with a fixed set of seven bases:
      2, 325, 9375, 28178, 450775, 9780504 and 1795265022. Each base
      is a quick check that every prime passes. A composite number
      can sometimes pass one check by luck, but this set of seven was
      found by Jim Sinclair in 2011 and tested by computer against
      every number below 2^64: no composite in that range passes all
      seven. So for a uint64_t, passing all seven means prime.

    Further reading:
    - https://en.wikipedia.org/wiki/Euclid%27s_Elements
    - https://en.wikipedia.org/wiki/Sieve_of_Eratosthenes
    - https://en.wikipedia.org/wiki/Trial_division
    - https://en.wikipedia.org/wiki/Miller%E2%80%93Rabin_primality_test
      (see "Testing against small sets of bases")
*/

#define PN_MATH_SIEVE_MAX_VALUE 1000000ull
#define PN_MATH_M_MAX_VALUE     65535ull
#define PN_MATH_SIDE_MAX_VALUE  4294967295ull
#define PN_MATH_MAX_FACTORS     64u

/// @returns how many primes there are from 2 to @param limit, the
/// number of slots pn_math_sieve needs.
size_t pn_math_sieve_size(uint64_t limit);

/// List the primes from 2 to @param limit with the sieve of
/// Eratosthenes, smallest first.
/// https://en.wikipedia.org/wiki/Sieve_of_Eratosthenes
///
/// @param limit - the largest number to check
/// @param primes - where the primes are written, with @param capacity slots
/// @param capacity - how many slots primes has; pn_math_sieve_size(limit)
///     is enough
/// @param error - where to put the error report on failure:
///     PN_ERROR_RANGE when limit is above PN_MATH_SIEVE_MAX_VALUE,
///     PN_ERROR_BUFFER when capacity is less than the number of primes.
///     On failure primes is not changed.
/// @returns how many primes were written, or 0 and the
/// @param error encountered (check the error, since there are no
///     primes below 2).
size_t pn_math_sieve(uint64_t limit, uint64_t* _Nonnull primes, size_t capacity,
                     pn_error* _Nullable* _Nullable error);

/// The greatest common divisor of @param a and @param b: the biggest
/// number that divides both. Uses Euclid's algorithm (Elements,
/// Book VII): replace the bigger number with the remainder of
/// dividing it by the smaller, until the remainder is 0.
/// gcd(1071, 462) = 21. gcd(0, 0) is 0 by convention.
/// https://en.wikipedia.org/wiki/Euclidean_algorithm
///
/// @returns the greatest common divisor of the
/// @param a and
/// @param b provided
uint64_t pn_math_greatest_common_divisor(uint64_t a, uint64_t b);

/// The least common multiple of @param a and @param b: the smallest
/// number both divide into, computed as a / gcd(a, b) x b.
/// lcm(1071, 462) = 23562. lcm(0, n) is 0 by convention.
/// https://en.wikipedia.org/wiki/Least_common_multiple
///
/// @returns the least common multiple of the
/// @param a and
/// @param b provided, or 0 and the
/// @param error encountered:
///     PN_ERROR_RANGE when the answer is too big for a uint64_t.
uint64_t pn_math_least_common_multiple(uint64_t a, uint64_t b,
                                       pn_error* _Nullable* _Nullable error);

/// Is @param number a square number (k x k for some whole k)? The
/// Pythagoreans drew square numbers as square arrays of dots.
/// is_square(999950884) is true: it is 31622 x 31622.
/// https://en.wikipedia.org/wiki/Square_number
///
/// @returns true when the
/// @param number provided is a square
bool pn_math_is_square(uint64_t number);

/// Is @param number prime? The answer is exact for every uint64_t:
/// trial division below 2^32, and Miller-Rabin with seven fixed
/// bases above (see the Math notes above). An easy test that only
/// checks Fermat's little theorem is fooled by Carmichael numbers
/// such as 561 = 3 x 11 x 17; this one is not, so is_prime(561) is
/// false.
/// https://en.wikipedia.org/wiki/Carmichael_number
///
/// @returns true when the
/// @param number provided is prime
bool pn_math_is_prime(uint64_t number);

/// The smallest prime that is at least @param number, so a prime is
/// its own answer: next_prime(1) = 2, next_prime(2) = 2,
/// next_prime(46340) = 46349.
///
/// @returns the next prime from the
/// @param number provided, or 0 and the
/// @param error encountered:
///     PN_ERROR_RANGE when there is no prime from number up to
///     UINT64_MAX.
uint64_t pn_math_next_prime(uint64_t number, pn_error* _Nullable* _Nullable error);

/// @returns how many prime factors @param n has, counting repeats
/// (90 = 2 x 3 x 3 x 5 has 4), the number of slots pn_math_factorize
/// needs. 0 and 1 have none.
size_t pn_math_factorize_size(uint64_t n);

/// Split @param number into its prime factors, smallest first, with
/// repeats: 90 = 2 x 3 x 3 x 5, 649 = 11 x 59. Every whole number
/// above 1 has exactly one such list (the fundamental theorem of
/// arithmetic). 0 and 1 give an empty list.
/// https://en.wikipedia.org/wiki/Fundamental_theorem_of_arithmetic
///
/// @param number - the number to factor
/// @param factors - where the factors are written, with @param capacity
///     slots
/// @param capacity - how many slots factors has;
///     pn_math_factorize_size(number) is enough
/// @param error - where to put the error report on failure:
///     PN_ERROR_BUFFER when capacity is less than the number of
///     factors. On failure factors is not changed.
/// @returns how many factors were written, or 0 and the
/// @param error encountered (check the error, since 0 and 1 have no
///     factors).
size_t pn_math_factorize(uint64_t number, uint64_t* _Nonnull factors, size_t capacity,
                         pn_error* _Nullable* _Nullable error);

/// Is @param number a perfect number, equal to the sum of its
/// divisors other than itself? 6 = 1 + 2 + 3 is the first; then come
/// 28, 496 and 8128. Euclid showed how to build even perfect numbers
/// (Elements, Book IX, Proposition 36), and Euler proved every even
/// perfect number has that form. Nobody knows if an odd one exists.
/// is_perfect(90) is false; is_perfect(0) and (1) are errors.
/// https://en.wikipedia.org/wiki/Perfect_number
///
/// @returns true when the
/// @param number provided is perfect, or false and the
/// @param error encountered:
///     PN_ERROR_RANGE when number is below 2.
bool pn_math_is_perfect(uint64_t number, pn_error* _Nullable* _Nullable error);

// MARK: - Pythagoras

/// @struct `pn_math_triple`: the sides of a right triangle
/// @field leg1 - one short side
/// @field leg2 - the other short side
/// @field hypotenuse - the long side, opposite the right angle
typedef struct pn_math_triple {
    uint64_t leg1;
    uint64_t leg2;
    uint64_t hypotenuse;
} pn_math_triple;

/// Make a Pythagorean triple (whole-number sides of a right
/// triangle) with Euclid's formula, from two numbers
/// @param m > @param n >= 1:
///
///     leg1 = m^2 - n^2,  leg2 = 2mn,  hypotenuse = m^2 + n^2
///
/// m = 2, n = 1 gives 3, 4, 5. m = 12, n = 5 gives 119, 120, 169, the
/// first row of the Babylonian tablet Plimpton 322.
/// https://en.wikipedia.org/wiki/Pythagorean_triple
/// https://en.wikipedia.org/wiki/Plimpton_322
///
/// @returns the triple made from the
/// @param m and
/// @param n provided, or a zeroed triple and the
/// @param error encountered:
///     PN_ERROR_RANGE when not 1 <= n < m <= PN_MATH_M_MAX_VALUE, or
///     a side is above PN_MATH_SIDE_MAX_VALUE.
pn_math_triple pn_math_pythagorean_triple(uint64_t m, uint64_t n,
                                          pn_error* _Nullable* _Nullable error);

/// Is leg1^2 + leg2^2 = hypotenuse^2, so that the
/// @param triple provided is a right triangle (the Pythagorean
/// theorem)? (119, 120, 169) is.
/// https://en.wikipedia.org/wiki/Pythagorean_theorem
///
/// @returns true when the triple is a right triangle, or false and
/// the
/// @param error encountered:
///     PN_ERROR_RANGE when a side is below 1 or above
///     PN_MATH_SIDE_MAX_VALUE.
bool pn_math_is_pythagorean_triple(pn_math_triple triple, pn_error* _Nullable* _Nullable error);

#pragma clang diagnostic pop

#ifdef __cplusplus
}
#endif
#endif /* PANTHEON_H */
