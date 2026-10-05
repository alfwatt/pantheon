/*  pantheon.h — libPantheon: on beyond math.h.

    Documented functions from the history of mathematics and science,
    efficient enough for demanding applications.

    Success costs nothing: no heap allocation, small by-value types,
    straight-line code.  A failure is the one moment the library
    allocates: it hangs a pn_error note on the caller's error slot,
    and the caller frees the note when done reading it.

    Every function that can fail takes a 'pn_error **error' slot as
    its last argument and returns true on success.  Before calling,
    the caller sets its own 'pn_error *' variable to NULL.  On
    success the slot stays NULL and the result lands in the out
    parameters.  On failure the function returns false, the note
    lands in the slot, and the out parameters keep their old values.

    This revision: Babylonian sexagesimal (base-60) arithmetic and
    cuneiform numerals; Egyptian hieroglyphic numerals; Chinese hanzi
    numerals and counting rods; Indian Brahmi numerals.  See README.md
    for the roadmap.
*/
#ifndef PANTHEON_H
#define PANTHEON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(__clang__) || !__has_feature(nullability)
#define _Nullable
#define _Nonnull
#endif

#if defined(__clang__) || defined(__GNUC__)
#define PN_FORMAT_STRING(fmt_idx, first_vararg)                                 \
    __attribute__((format(printf, fmt_idx, first_vararg)))
#else
#define PN_FORMAT_STRING(fmt_idx, first_vararg)
#endif

// MARK: - Errors

/*
  A failed call leaves behind a note that explains itself.  The note
  carries three things: a code that names the kind of failure, a
  message that explains it in a sentence a person can read, and a
  pointer to the earlier note that caused it, when there was one.

  Callers never build notes by hand.  A function that fails calls
  pn_error_set (or pn_error_prepend, when an inner failure deserves a
  second sentence on top), which allocates the note for the caller.
  A caller reads the note with pn_error_is or pn_error_describe and
  hands it back with pn_error_free or pn_error_clear.
*/

/// @enum `pn_error_code`: the five ways a call can fail
typedef enum pn_error_code {
    PN_ERROR_NULL,   /* a pointer argument arrived NULL              */
    PN_ERROR_SYNTAX, /* an input string breaks the writing rules     */
    PN_ERROR_RANGE,  /* a number is too big for this number system   */
    PN_ERROR_DOMAIN, /* a number of the wrong kind for this job      */
    PN_ERROR_BUFFER, /* the caller's buffer is too small             */
} pn_error_code;

/// @struct `pn_error`: the note a failed call leaves behind
/// @field code - the error code, one of the `PN_ERROR_` values above
/// @field message - a human readable sentence describing the error,
///     may be NULL when the code alone says everything
/// @field prior - a pointer to a prior error that caused this error,
///     may be NULL
typedef struct pn_error {
    pn_error_code    code;
    char            *_Nullable message;
    struct pn_error *_Nullable prior;
} pn_error;

/// @returns a new, writable `pn_error` with the
/// @param code provided,
/// @param message copied into the note (or with no message when the
///     @param message is NULL), and no prior note.
/// Aborts when memory runs out: a failure to report a failure has no
/// second way to speak.  Free the result with pn_error_free.
pn_error *_Nonnull pn_error_init(pn_error_code code,
                                 const char *_Nullable message);

/// @returns a new note that reads "this happened, because that
/// happened": the @param code and @param message provided, with the
/// @param prior note — whatever caused this failure — chained behind
/// it.  The new note owns @param prior and frees it together with
/// itself.  Free the result with pn_error_free.
pn_error *_Nonnull pn_error_prepend_note(pn_error *_Nullable prior,
                                         pn_error_code code,
                                         const char *_Nullable message);

/// Attaches a fresh note to the caller's @param location and tells
/// the caller the call failed.  The note carries the @param code and
/// a message built from the @param format string and its arguments,
/// printf-style.  Passing a NULL @param location is legal: the
/// message is then simply not kept.  A note already sitting in
/// @param location is freed first, so only the newest note survives.
/// @returns false, always, so a failing function can end with
///     'return pn_error_set(...);'.
bool pn_error_set(pn_error *_Nullable *_Nullable location,
                  pn_error_code code,
                  const char *_Nonnull format,
                  ...) PN_FORMAT_STRING(3, 4);

/// Moves the @param cause note (built by inner code, typically a
/// tokenizer naming the exact byte and position) into the caller's
/// @param location as a new note with the @param code and a message
/// built from the @param format string and its arguments, printf
/// style.  The new note owns @param cause and frees it together with
/// itself.  A note already in @param location is freed first; a NULL
/// @param location simply drops both sentences.
/// @returns false, always, so a failing function can end with
///     'return pn_error_prepend(...);'.
bool pn_error_prepend(pn_error *_Nullable *_Nullable location,
                      pn_error *_Nullable cause,
                      pn_error_code code,
                      const char *_Nonnull format,
                      ...) PN_FORMAT_STRING(4, 5);

/// Frees the @param error and every note chained behind it.  Passing
/// NULL is legal and does nothing.
void pn_error_free(pn_error *_Nullable error);

/// @returns true when the @param error exists and carries exactly
/// the @param code, false otherwise — including when @param error is
/// NULL.  This is how callers ask what went wrong without reading
/// prose.
bool pn_error_is(const pn_error *_Nullable error, pn_error_code code);

/// A printable sentence for printf("%s", ...) and for test output.
/// @returns the @param error's own message when it has one, the
/// name of the code (like "range") when it has none, and "no error"
/// when the @param error is NULL.  Never NULL; the sentence stays
/// alive exactly as long as the @param error does, so read it before
/// freeing.
const char *_Nonnull pn_error_describe(const pn_error *_Nullable error);

/// Frees any note sitting in the caller's @param location and
/// leaves NULL behind.  Passing a NULL @param location is legal and
/// does nothing.
void pn_error_clear(pn_error *_Nullable *_Nullable location);

/*  Legacy status codes still used by the numeral modules being
    converted this revision; each module that adopts pn_error drops
    its share.  Error text for printf("%s", pn_strerror(st)).  Never
    NULL.  */
typedef enum pn_status {
    pn_ok = 0,
    pn_err_null,    /* a pointer argument was NULL                    */
    pn_err_syntax,  /* malformed input string                         */
    pn_err_range,   /* too many places, a place > 59, or the value
                       does not fit the target type                   */
    pn_err_domain,  /* value not representable in the requested form
                       (e.g. fractional places converted to integer)  */
    pn_err_buffer,  /* caller's buffer too small                      */
} pn_status;

const char *pn_strerror(pn_status st);

// MARK: - Sexagesimal

/* ------------------------------------------------------------------------
 * Babylonian sexagesimal (base-60) values.
 *
 * A value is a string of "places", each holding a digit 0..59, most
 * significant first.  The Babylonians (c. 1800 BCE onward) wrote these
 * with two cuneiform signs and — crucially — had NO zero placeholder
 * and NO radix point: 𒁹 (one vertical wedge) alone could mean 1, 60
 * or 1/60; context decided.  See
 * https://en.wikipedia.org/wiki/Sexagesimal and
 * https://en.wikipedia.org/wiki/Babylonian_numerals .
 *
 * pn_sex60 therefore carries an explicit radix position, a MODERN
 * convention added so this type is a number and not a riddle:
 *
 *     value = (-1)^neg * sum_i  places[i] * 60^(radix - 1 - i)
 *
 * In period style, use radix == n_places (a pure integer).
 *
 * The representation is a fixed-size array of bytes inside a ~20-byte
 * struct passed by value: no allocations, safe for real-time and
 * embedded callers.  double is offered only as a conversion utility,
 * never the canonical form — many exact sexagesimal values (0;20 is
 * 1/3) have no finite binary expansion, and 60^9 already exceeds 2^53,
 * beyond the exact range of a double.
 * ---------------------------------------------------------------------- */

#define PN_SEX60_MAX_PLACES 16  /* 60^16 ~ 2.8e28: plenty; keeps the struct tiny */

typedef struct pn_sex60 {
    uint8_t places[PN_SEX60_MAX_PLACES]; /* each 0..59, most significant first */
    uint8_t n_places;                     /* number of significant places, 1..16 */
    uint8_t radix;                        /* places left of the radix point
                                             (a modern addition — the Babylonians
                                             wrote none); 0 means purely fractional */
    bool    neg;                          /* modern nicety; Old Babylonian texts
                                             carry no sign */
} pn_sex60;

/* --- sexagesimal strings (modern scholarly transcription) --------------
 *
 * Old Babylonian values survive in print as comma- or colon-separated
 * places, e.g. the famous YBC 7289 diagonal:  1,24,51,10  (= about 1.41421,
 * an excellent approximation of sqrt(2)).  Following Neugebauer, we let a
 * semicolon mark the radix point — the convention for readings like
 * 1;24,51,10 — though no Babylonian tablet ever wrote one.
 * -------------------------------------------------------------------- */

/// Read a modern transcription of a sexagesimal value: an optional
/// '-', then one or more places of one or two decimal digits (each
/// 0 to 59) joined by ',' ':' or ';'.  At most one ';' may appear,
/// and it marks where the radix point sits:
///
///      "1,24,51,10"  -> places {1,24,51,10}, radix 4(period-style whole number)
///      "1;24,51,10"  -> the same places, radix 1(about 1.414...)
///      "0;45"        -> places {0,45}, radix 1(equals 3/4; the 0 is read literally)
///
/// The reading is strict: no spaces, no empty places, no trailing
/// separator.  One trap: an unbroken "3599" is NOT the decimal number
/// 3599 — it reads as the places 35 and 99, and 99 is above 59, so it
/// is refused.  Bring ordinary whole numbers in through
/// pn_sex60_from_u64 instead.
/// @param text - the transcription to read
/// @param out - where the value lands on success; untouched on any
///     failure
/// @param error - where to hang the note on failure: PN_ERROR_NULL
///     when text or out is NULL, PN_ERROR_SYNTAX when the grammar is
///     broken, PN_ERROR_RANGE when a place tops 59, a place wears
///     more than 2 digits, or the value grows past
///     PN_SEX60_MAX_PLACES places.
/// @returns true on success, false with a note in @param error.
bool pn_sex60_parse(const char *text, pn_sex60 *_Nullable out,
                    pn_error *_Nullable *_Nullable error);

/// Write the value back out as a transcription.  @param sep must be
/// ',' ':' or ';'.  Places are written with two digits — only the
/// first place drops its leading zero — joined with @param sep,
/// except the radix boundary, which always writes ';' when
/// 0 < radix < n_places:
///
///       {1,24,51,10} radix 1  with sep ','   ->     "1;24,51,10"
///       {2,23,3}     radix 3  with sep ','   ->     "2,23,03"
///
/// One caveat: with sep ';' and an interior radix the output carries
/// two ';' and will not parse back — use ',' or ':' when the string
/// has to survive the round trip.  A negative value grows a leading
/// '-'.  The write ends with a NUL and needs at most
/// PN_SEX60_FORMAT_MAX_LEN bytes.
/// @param v - the value to write out
/// @param sep - the joiner between places: ',' ':' or ';'
/// @param buf - where the text lands, with room for the NUL
/// @param buflen - bytes of room at buf
/// @param error - where to hang the note on failure: PN_ERROR_NULL
///     when buf is NULL, PN_ERROR_SYNTAX when sep is not one of the
///     three joiners or (with ';' and an interior radix) the output
///     would wear two radix points, PN_ERROR_DOMAIN when v is
///     malformed, PN_ERROR_BUFFER when buflen is too small.  On any
///     failure buf is left untouched.
/// @returns true on success, false with a note in @param error.
#define PN_SEX60_FORMAT_MAX_LEN (1 + 3 * PN_SEX60_MAX_PLACES + 1) /* '-' + places+seps + NUL */
bool pn_sex60_format(pn_sex60 v, char sep, char *_Nonnull buf,
                     size_t buflen, pn_error *_Nullable *_Nullable error);

/* --- conversion to / from ordinary numbers ------------------------------ */

/// Turn an ordinary unsigned integer into sexagesimal places by
/// dividing by 60 again and again:
///
///        8583 -> places {2,23,3}, radix 3    (8583 = 2*60^2 + 23*60 + 3;
///                                           Wikipedia's worked example)
///        0     -> a single place {0}
/// @param n - the whole number to convert
/// @param out - where the value lands on success
/// @param error - where to hang the note on failure: PN_ERROR_NULL
///     when out is NULL, PN_ERROR_RANGE when the number would need
///     more than PN_SEX60_MAX_PLACES places (in practice unreachable:
///     2^64 needs only 11 places).
/// @returns true on success, false with a note in @param error.
bool pn_sex60_from_u64(uint64_t n, pn_sex60 *_Nullable out,
                       pn_error *_Nullable *_Nullable error);

/// Turn a sexagesimal whole number back into an ordinary unsigned
/// integer with Horner's scheme (acc = acc*60 + place).  The value
/// must have radix == n_places, that is: no fraction.
/// @param v - the value to convert; it must be well-formed
/// @param out - where the number lands on success
/// @param error - where to hang the note on failure: PN_ERROR_NULL
///     when out is NULL, PN_ERROR_DOMAIN when v is malformed or
///     carries any fractional place (a place above 59 counts as
///     malformed too), PN_ERROR_RANGE when the value does not fit a
///     uint64_t.
/// @returns true on success, false with a note in @param error.
bool pn_sex60_to_u64(pn_sex60 v, uint64_t *_Nullable out,
                     pn_error *_Nullable *_Nullable error);

/// Turn a sexagesimal value into a double.  The fold runs
/// right-to-left, val = (val + place) / 60, so whole values stay
/// exact while the whole part fits inside 2^53 (radix <= 8, because
/// 60^8 ~ 1.7e14).
/// Like every jump to binary floating point this cannot keep every
/// sexagesimal value: 0;20 (which is 1/3) has no finite binary
/// expansion, and past 2^53 answers are correctly rounded yet not
/// exact.
/// @param v - the value to convert; it must be well-formed
/// @param out - where the double lands on success
/// @param error - where to hang the note on failure: PN_ERROR_NULL
///     when out is NULL, PN_ERROR_DOMAIN when v is malformed.
/// @returns true on success, false with a note in @param error.
bool pn_sex60_to_double(pn_sex60 v, double *_Nullable out,
                        pn_error *_Nullable *_Nullable error);

/// Turn a double into sexagesimal places with exactly
/// @param frac_places fractional places (0 to PN_SEX60_MAX_PLACES),
/// rounded half away from zero at the last kept place with carries
/// walking left.  Trailing zero places are KEPT, never trimmed, so
/// every answer is reproducible:
///
///        0.75 with 1 frac place  ->  {45}      radix 0    (0;45 = 3/4)
///        1.5  with 1 frac place  ->  {1,30}    radix 1    (1;30)
///
/// The Babylonians would have dropped trailing empty places — and
/// could afford to, since context told the reader the size anyway.
/// NaN and infinity are refused; see pn_sex60_to_double for the
/// general exactness caveats.
/// @param x - the number to convert
/// @param frac_places - how many fractional places to keep
/// @param out - where the value lands on success
/// @param error - where to hang the note on failure: PN_ERROR_NULL
///     when out is NULL, PN_ERROR_DOMAIN for NaN, infinity, or
///     frac_places above the maximum, PN_ERROR_RANGE when the whole
///     part or the rounded result needs more than
///     PN_SEX60_MAX_PLACES places.
/// @returns true on success, false with a note in @param error.
bool pn_sex60_from_double(double x, uint8_t frac_places,
                          pn_sex60 *_Nullable out,
                          pn_error *_Nullable *_Nullable error);

// MARK: - Cuneiform Numbers

/* --- Babylonian cuneiform numerals (UTF-8 rendering) --------------------
 *
 * The two positional signs of the Neo-Babylonian system, per the
 * Unicode Standard names list:
 *
 *   U+12079  CUNEIFORM SIGN DISH   𒁹   vertical wedge, value 1
 *            UTF-8: F0 92 81 B9
 *   U+1230B  CUNEIFORM SIGN U      𒌋   corner wedge,   value 10
 *            UTF-8: F0 92 8C 8B
 *
 * Both live in the main Cuneiform block (U+12000..U+123FF); the
 * precomposed signs of the "Cuneiform Numbers and Punctuation" block
 * (U+12400..) are metrology signs, not school-text positional digits,
 * so we do NOT use them — each place is written as (d/10) copies of U
 * followed by (d%10) copies of DISH, e.g. 23 = 𒌋𒌋𒁹𒁹𒁹.
 *
 * Unicode encodes NO zero placeholder for the sexagesimal gap.  An
 * empty place is rendered as two spaces — faithfully reproducing the
 * original place-value ambiguity: on the clay tablet a lone 𒁹 was 1 or
 * 60 or 1/60, and only context told the reader which.
 * -------------------------------------------------------------------- */

#define PN_CUNEIFORM_DISH 0x12079u /* 𒁹 vertical wedge = 1  */
#define PN_CUNEIFORM_U    0x1230Bu /* 𒌋 corner wedge    = 10 */

/* Exact number of bytes (excluding the NUL) pn_babylonian_render will
 * write for a well-formed v.  Pure and total — it never fails; on a
 * malformed v its answer is simply meaningless, since render rejects
 * such values with a PN_ERROR_DOMAIN note. */
size_t pn_babylonian_render_size(pn_sex60 v);

/// Write @param v in positional cuneiform style into @param buf as
/// UTF-8, ending with a NUL.  Each place becomes a group of (d/10) U
/// signs plus (d%10) DISH signs; groups sit one space apart; a place
/// of zero is the two-space gap the scribes left; a negative value
/// grows a leading '-', which is an anachronism.  Any well-formed
/// value renders — the caller decides which places are "really"
/// there.  No radix point is drawn (no tablet ever wore one), and
/// fractional places render exactly like whole ones.
/// @param v - the value to render
/// @param buf - where the cuneiform lands, with room for the NUL
/// @param buflen - bytes of room at buf
/// @param error - where to hang the note on failure: PN_ERROR_NULL
///     when buf is NULL, PN_ERROR_DOMAIN when v is malformed
///     (n_places == 0 or above the max, a place past 59, or radix
///     past n_places), PN_ERROR_BUFFER when buflen does not beat
///     pn_babylonian_render_size(v).  On any failure buf is left
///     untouched.
/// @returns true on success, false with a note in @param error.
bool pn_babylonian_render(pn_sex60 v, char *_Nonnull buf, size_t buflen,
                          pn_error *_Nullable *_Nullable error);

// Parse a rendered cuneiform string — the inverse of
// pn_babylonian_render.  @param buf holds exactly @param buflen bytes
// of UTF-8 built from '-', the two signs, and separating spaces:
//
//          𒁹𒁹 𒌋𒌋𒁹𒁹𒁹 𒁹𒁹𒁹             ->    {2,23,3}
//
// A place group is (d/10) U signs then (d%10) DISH signs; groups join
// with one space, and the two-space gap of a zero place reads back as
// a zero place.  The spacing is strict — between signs it must run 1,
// 4, 7, … spaces (3k+1: joiner plus k gaps); leading and trailing
// runs are multiples of 3; an all-space string has length 3k-1.  Any
// other spacing, and any other byte, is malformed.  Round-trip law:
// pn_babylonian_parse(buf, strlen(buf), &w, &err) after a successful
// pn_babylonian_render(v, buf, …) recovers v's places exactly.
//
// The cuneiform itself carries no radix point ('-' notwithstanding),
// so the parsed value gets the period-style reading: radix ==
// n_places.  A lone 𒁹 parses as 1 — whether the scribe meant 60 or
// 1/60, only context knew.
/// @param buf - the rendered cuneiform to read
/// @param buflen - bytes to read at buf
/// @param out - where the value lands on success; untouched on any
///     failure
/// @param error - where to hang the note on failure: PN_ERROR_NULL
///     when buf or out is NULL, PN_ERROR_SYNTAX for any foreign byte
///     or illegal spacing, including an empty string and a DISH
///     before a U in one group, PN_ERROR_RANGE for a group with more
///     than 5 U or 9 DISH signs, or for a value past
///     PN_SEX60_MAX_PLACES places.
/// @returns true on success, false with a note in @param error.
bool pn_babylonian_parse(const char *_Nullable buf, size_t buflen,
                         pn_sex60 *_Nullable out,
                         pn_error *_Nullable *_Nullable error);

// MARK: - Vigesimal

/* ------------------------------------------------------------------------
 *  Vigesimal — the base-20 core value: places, radix, conversions.
 *
 *  The pn_vigesimal counterpart of pn_sex60: a fixed-size array of
 *  bytes inside a ~20-byte struct passed by value — no allocations,
 *  safe for real-time and embedded callers.
 *
 *      value = (-1)^neg * sum_i  places[i] * 20^(radix - 1 - i)
 *
 *  places[i] runs 0..19, most significant first.  In period style,
 *  use radix == n_places.  The radix position and the sign are MODERN
 *  niceties, added so this type is a number and not a riddle: no
 *  book of the codices wrote a radix point, and the Dresden Codex
 *  carries no negative numbers.
 *
 *  double is offered only as a conversion utility, never the canonical
 *  form: 0;5 in this notation is 1/4 and exact, but 0;1 is 1/20 and
 *  has no finite binary expansion (20 = 4 x 5 keeps a factor five),
 *  and 20^13 = 8.192e16 already exceeds 2^53 — one whole row later
 *  than sexagesimal's 60^8, because base twenty is half a square.
 *  ---------------------------------------------------------------------- */

#define PN_VIGESIMAL_MAX_PLACES 16    /* 20^16 ~ 6.6e20: plenty; keeps the struct tiny */

typedef struct pn_vigesimal {
	uint8_t places[PN_VIGESIMAL_MAX_PLACES]; /* each 0..19, most significant first */
	uint8_t n_places;                     /* number of significant places, 1..16 */
	uint8_t radix;                        /* places left of the radix point
	                                         (a modern addition — the Maya
	                                         wrote none); 0 means purely fractional */
	bool    neg;                          /* modern nicety; the codices
	                                         carry no sign */
} pn_vigesimal;

/// Read a modern transcription of a vigesimal value — the same
/// reading convention as the sexagesimal transcriptions above:
/// decimal places joined by ',' or ':', with at most one ';' marking
/// where the radix point sits.  No Maya scribe ever wrote any of it.
///
///        "1,2,0,0"   -> places {1,2,0,0}, radix 4  (period-style = 8800)
///        "1;12,9"    -> the same places, radix 1   (= 1 + 12/20 + 9/400)
///        "0;5"       -> places {0,5}, radix 1      (= 1/4; the 0 is
///                                                   read literally)
///
/// The reading is strict: no spaces, no empty places, no trailing
/// separator.  One trap: an unbroken "399" is NOT the decimal number
/// 399 — it reads three digits deep and is refused as out of range
/// (a longer run of digits is refused the same way).  Bring ordinary
/// whole numbers in through pn_vigesimal_from_u64 instead.
/// @param text - the transcription to read
/// @param out - where the value lands on success; untouched on any
///     failure
/// @param error - where to hang the note on failure: PN_ERROR_NULL
///     when text or out is NULL, PN_ERROR_SYNTAX when the grammar is
///     broken, PN_ERROR_RANGE when a place tops 19, a place wears
///     more than 2 digits, or the value grows past
///     PN_VIGESIMAL_MAX_PLACES places.
/// @returns true on success, false with a note in @param error.
bool pn_vigesimal_parse(const char *text, pn_vigesimal *_Nullable out,
                        pn_error *_Nullable *_Nullable error);

#define PN_VIGESIMAL_FORMAT_MAX_LEN (1 + 3 * PN_VIGESIMAL_MAX_PLACES + 1) /* '-' + places+seps + NUL */

/// Write @param v out as a transcription joined with @param sep
/// (',' or ':'; the radix boundary itself writes ';' whenever the
/// radix sits in the middle of the value).  The write ends with a
/// NUL and stays within PN_VIGESIMAL_FORMAT_MAX_LEN bytes.
/// @param v - the value to write out
/// @param sep - the joiner between places
/// @param buf - where the text lands, with room for the NUL
/// @param buflen - bytes of room at buf
/// @param error - where to hang the note on failure: PN_ERROR_NULL
///     when buf is NULL, PN_ERROR_SYNTAX when sep is not one of the
///     allowed joiners, PN_ERROR_DOMAIN when v is malformed
///     (n_places == 0 or past the max, a place above 19, radix past
///     n_places), PN_ERROR_BUFFER when buflen is too small.  On any
///     failure buf is left untouched.
/// @returns true on success, false with a note in @param error.
bool pn_vigesimal_format(pn_vigesimal v, char sep, char *_Nonnull buf,
                         size_t buflen, pn_error *_Nullable *_Nullable error);

/// Turn an ordinary unsigned integer into vigesimal places exactly
/// how the sexagesimal from_u64 does it, rebased: peel 20s off the
/// bottom and flip the order.   @param n == 0 becomes a single place
/// of 0.
/// @param n - the whole number to convert
/// @param out - where the value lands on success
/// @param error - where to hang the note on failure: PN_ERROR_NULL
///     when out is NULL, PN_ERROR_RANGE when the number would need
///     more than PN_VIGESIMAL_MAX_PLACES places (in practice
///     unreachable: UINT64_MAX takes 15 places).
/// @returns true on success, false with a note in @param error.
bool pn_vigesimal_from_u64(uint64_t n, pn_vigesimal *_Nullable out,
                           pn_error *_Nullable *_Nullable error);

/// Build a vigesimal whole number back up with Horner's scheme.
/// @param v - the value to convert; well-formed and whole
/// @param out - where the number lands on success
/// @param error - where to hang the note on failure: PN_ERROR_NULL
///     when out is NULL, PN_ERROR_DOMAIN when v is malformed or its
///     radix is not n_places (that is: not a whole number),
///     PN_ERROR_RANGE when the value does not fit a uint64_t
///     (16 places maxed out = 20^16 - 1 would not).
/// @returns true on success, false with a note in @param error.
bool pn_vigesimal_to_u64(pn_vigesimal v, uint64_t *_Nullable out,
                         pn_error *_Nullable *_Nullable error);

/// Turn a vigesimal value into a double: the whole part runs through
/// Horner's scheme in exact uint64_t arithmetic while it fits, then
/// one correctly-rounded division F / 20^k while k <= 14.  Results
/// are exact whenever a double can hold them at all.   (A value that
/// is purely fractional past 14 places drops to the iterated
/// division loop; harmless.)
/// @param v - the value to convert; it must be well-formed
/// @param out - where the double lands on success
/// @param error - where to hang the note on failure: PN_ERROR_NULL
///     when out is NULL, PN_ERROR_DOMAIN when v is malformed.
/// @returns true on success, false with a note in @param error.
bool pn_vigesimal_to_double(pn_vigesimal v, double *_Nullable out,
                            pn_error *_Nullable *_Nullable error);

/// Turn a double into vigesimal places, rounding half away from zero
/// at the @param frac_places-th place (each place 0..19), just like
/// pn_sex60_from_double: places kept past the rounding are zeros,
/// 0.98 rounded at one place steps up to a whole 1, NaN and infinity
/// are refused.
/// @param x - the number to convert
/// @param frac_places - how many fractional places to keep
/// @param out - where the value lands on success
/// @param error - where to hang the note on failure: PN_ERROR_NULL
///     when out is NULL, PN_ERROR_DOMAIN for NaN, infinity, or
///     frac_places past PN_VIGESIMAL_MAX_PLACES, PN_ERROR_RANGE when
///     the carries grow the value past 16 places.
/// @returns true on success, false with a note in @param error.
bool pn_vigesimal_from_double(double x, uint8_t frac_places,
                              pn_vigesimal *_Nullable out,
                              pn_error *_Nullable *_Nullable error);

/* ------------------------------------------------------------------------
 * Egyptian hieroglyphic numerals (UTF-8 rendering).
 *
 * The Egyptian system (c. 3000 BCE onward) was decimal but ADDITIVE: a
 * distinct hieroglyph for each power of ten, repeated as many times as
 * the digit demands — no positional notation at all.  See
 * https://en.wikipedia.org/wiki/Egyptian_numerals .  The seven signs,
 * per the Unicode Standard names list (all in the Egyptian Hieroglyphs
 * block U+13000..U+1342F, all 4 bytes of UTF-8):
 *
 *   U+133E4  EGYPTIAN HIEROGLYPH Z001   𓏤  single stroke,      value 1
 *   U+13386  EGYPTIAN HIEROGLYPH V020   𓎆  cattle hobble,        value 10
 *   U+13362  EGYPTIAN HIEROGLYPH V001   𓍢  coil of rope,         value 100
 *   U+131BC  EGYPTIAN HIEROGLYPH M012   𓆼  lotus,                value 1 000
 *   U+130AD  EGYPTIAN HIEROGLYPH D050   𓂭  bent finger,          value 10 000
 *   U+13190  EGYPTIAN HIEROGLYPH I008   𓆐  tadpole,              value 100 000
 *   U+13068  EGYPTIAN HIEROGLYPH C011   𓁨  Heh (god, arms raised), value 1 000 000
 *
 * There is NO sign above the Heh and NO zero: the scribe simply left a
 * blank where our 0 sits, and a lone blank on an accounting ostracon
 * meant "nothing owed".  So the renderable domain is the scribe's full
 * set, 0..PN_EGYPTIAN_MAX_VALUE — every sign at most nine times — and
 * zero renders as the empty string.  Values beyond the Heh (10 000 000
 * would need ten Heh signs, 10^8 a hundred) are pn_err_range, not
 * because Egypt could not count them but because this rendering insists
 * each sign be written out, and a nine-million-sign tablet fits no
 * caller's buffer.
 *
 * Canonical form: signs grouped in DESCENDING powers of ten, one
 * contiguous run per sign, with no separators — unlike cuneiform, the
 * distinct signs are self-delimiting and there is no gap to preserve.
 * 4622 (the Karnak carving) is thus four lotus + six coils + two
 * hobbles + two strokes, largest first, as any scribe would carve it.
 * ---------------------------------------------------------------------- */

// MARK: - Egyptian Numerals

#define PN_EGYPTIAN_STROKE     0x133E4u  /* 𓏤 single stroke   = 1          */
#define PN_EGYPTIAN_HOBBLE     0x13386u  /* 𓎆 cattle hobble   = 10         */
#define PN_EGYPTIAN_COIL       0x13362u  /* 𓍢 coil of rope    = 100        */
#define PN_EGYPTIAN_LOTUS      0x131BCu  /* 𓆼 lotus           = 1 000      */
#define PN_EGYPTIAN_FINGER     0x130ADu  /* 𓂭 bent finger     = 10 000     */
#define PN_EGYPTIAN_TADPOLE    0x13190u  /* 𓆐 tadpole         = 100 000    */
#define PN_EGYPTIAN_HEH        0x13068u  /* 𓁨 Heh god         = 1 000 000  */
#define PN_EGYPTIAN_MAX_VALUE  9999999ull /* nine of each sign: the scribe's full set */

/// Count the bytes pn_egyptian_render will write for @param n, not
/// counting the NUL: four per sign drawn, which is 4 times the sum of
/// the decimal digits for @param n up to PN_EGYPTIAN_MAX_VALUE.  This
/// answer never fails; past the maximum the number is only
/// meaningless, because render turns down such values with a
/// PN_ERROR_RANGE note anyway.  @param n == 0 gives 0: the blank.
size_t pn_egyptian_render_size(uint64_t n);

/// Render @param n in hieroglyphic numerals into @param buf as UTF-8,
/// ending with a NUL: for each decimal digit d at 10^k, write d copies
/// of that power's sign, with the runs stepping down in power (10^6
/// first) and no separators between them.  @param n == 0 renders the
/// empty string — the faithful blank.  The buffer needs exactly
/// pn_egyptian_render_size(n) bytes plus the NUL.
/// @param buf - where the numeral is written; must not be NULL
/// @param buflen - bytes of room at buf; must exceed the render size
/// @param error - where to hang the note on failure: PN_ERROR_NULL
///     when buf is NULL, PN_ERROR_RANGE when n is past the scribe's
///     full set, PN_ERROR_BUFFER when buflen is too small.  On any
///     failure buf is left untouched.
/// @returns true on success, false with a note in @param error.
bool pn_egyptian_render(uint64_t n, char *_Nonnull buf, size_t buflen,
                        pn_error *_Nullable *_Nullable error);

/// Read back a rendered hieroglyphic string — the exact inverse of
/// pn_egyptian_render.  @param buf holds @param buflen bytes of UTF-8
/// built from the seven signs only, in canonical form: one unbroken
/// run per sign, the runs stepping down in power, each run holding 1
/// to 9 copies.  The value is the sum of what every sign is worth, so
/// the round trip is exact:  @param n = 0 (an empty string) reads back
/// as the nothing it was.  Any byte that is not a sign, a run that
/// climbs instead of steps down, or one sign split into two runs is
/// malformed — no sign was ever carved bottom-up on a temple wall.
/// @param buf - the string to read; NULL is refused with a note
/// @param buflen - bytes to read at buf
/// @param out - where the number lands on success; left untouched on
///     any failure
/// @param error - where to hang the note on failure: PN_ERROR_NULL
///     when buf or out is NULL, PN_ERROR_SYNTAX for a foreign byte,
///     a cut-off sign, or runs out of order, PN_ERROR_RANGE for a run
///     of ten or more copies of one sign — a scribe would have
///     carried to the next power, so that string is no canonical
///     number.
/// @returns true on success, false with a note in @param error.
bool pn_egyptian_parse(const char *_Nullable buf, size_t buflen,
                       uint64_t *_Nullable out,
                       pn_error *_Nullable *_Nullable error);

// MARK: - Hanzi Numerals

/* ------------------------------------------------------------------------
	*  Chinese hanzi numerals (UTF-8 rendering).
	*
	*  Chinese numerals are decimal, but they write no bare place-value
	*  digits: the number is two sections around 万, value = A * 10^4 + B,
	*  and each section spells its nonzero digits high to low as
	*  (digit + 千/百/十), the section's units digit bare and last:
	*
	*      四千六百二十二          = 4622      (Wikipedia's worked example)
	*      九千九百九十九万
	*      九千九百九十九            = 99 999 999  (the maximum)
	*
	*  The signs, per the Unicode Standard names list (all in the CJK
	*  Unified Ideographs block, all 3 bytes of UTF-8):
	*
	*    U+4E00 一 = 1      U+4E8C 二 = 2      U+4E09 三 = 3
	*    U+56DB 四 = 4      U+4E94 五 = 5      U+516D 六 = 6
	*    U+4E03 七 = 7      U+516B 八 = 8      U+4E5D 九 = 9
	*    U+5341 十 = 10     U+767E 百 = 100    U+5343 千 = 1000
	*    U+4E07 万 = 10 000
	*
	*  Zero follows the Egyptian blank: a skipped digit is simply not
	*  written, and the skip is acknowledged by exactly one U+0020 space
	*  between the surrounding terms — 1005 is 一千 五 and 1050 is
	*  一千 五十, where later China would write 一千零五 and 一千零五十.
	*  The space never carries value, because each digit's place is
	*  already pinned by its multiplier (a bare digit is always its
	*  section's units digit), so the round trip stays exact — unlike
	*  the load-bearing gap of the counting rods below.
	*
	*  Canonical form is the full formal reading; colloquial elisions are
	*  not canonical: 14 is 一十四, never the spoken 十四.  亿 (10^8) and
	*  traditional 萬 are outside this rendering — the domain stops at
	*  万, the same ruling as Egypt's missing sign above the Heh — so
	*  PN_HANZI_MAX_VALUE is 99 999 999, 九千九百九十九万九千九百九十九.
	* ---------------------------------------------------------------------- */

#define PN_HANZI_MAX_VALUE 99999999ull /* nine 九 in every place of both sections */

/*  Exact number of bytes (excluding the NUL) pn_hanzi_render will
	*  write for a well-formed n: three per sign drawn — every digit, every
	*  multiplier used, and the 万 — plus one for each blank space
	*  acknowledging a skipped digit.  Pure and total — it never fails; for
	*  n > PN_HANZI_MAX_VALUE its answer is simply meaningless, since
	*  render rejects such values with pn_err_range.  render_size(0) == 0:
	*  the blank. */
size_t pn_hanzi_render_size(uint64_t n);

/*  Render n in hanzi numerals into buf as UTF-8, NUL-terminated: write
	*  n = A * 10^4 + B; when A is nonzero, its section high to low (digit +
	*  千/百/十 terms, a bare digit last for the section's units place),
	*  then 万, then B's nonzero terms likewise; a skip of one or more
	*  places between consecutive terms is marked by exactly one space.
	*  n == 0 renders the empty string — the blank.  Needs exactly
	*  pn_hanzi_render_size(n) bytes + NUL; a smaller buffer gives
	*  pn_err_buffer and leaves buf untouched.
	*
	*  Errors: pn_err_null, pn_err_range (n > PN_HANZI_MAX_VALUE),
	*  pn_err_buffer.  On error buf is untouched. */
pn_status pn_hanzi_render(uint64_t n, char *buf, size_t buflen);

/*  Parse a rendered hanzi string — the inverse of pn_hanzi_render.
	*  Strict canonical form: digits 一..九, each followed by its
	*  multiplier (千/百/十) except a bare units digit last in its section;
	*  multipliers strictly descending within a section; at most one 万,
	*  immediately after A's last term and never before any B term:
	*
	*      四千六百二十二  ->  4622
	*      一千 五       ->  1005   (the blank: 百 and 十 skipped)
	*      四万一千      ->  41000   (no space: 万 and 千 adjacent)
	*
	*  Round-trip law, exact: parsing buf after a successful render
	*  recovers n.  An empty buffer (buflen == 0) parses as 0 — the blank
	*  reads back as the nothing it was.  The grammar cannot express a
	*  value past PN_HANZI_MAX_VALUE, so overflow needs no check; the
	*  colloquial 十四, a stray 零 or 〇, a space where adjacent terms
	*  meet (四百 三十一), a missing blank across a skip (五千万五),
	*  repeated or ascending multipliers, a digit after a section's bare
	*  digit, a second 万, or 万 after a multiplier are all
	*  pn_err_syntax.
	*
	*  Errors: pn_err_null, pn_err_syntax (any other byte, a cut-off sign,
	*  leading/trailing/double spaces, or the canonical violations above).
	*  On error *out is untouched. */
pn_status pn_hanzi_parse(const char *buf, size_t buflen, uint64_t *out);

/* ------------------------------------------------------------------------
	*  Counting rod numerals (UTF-8 rendering).
	*
	*  Rod numerals (算筹, in use c. 400 BCE onward in China, Japan,
	*  Korea and Vietnam) are true positional decimal: one bundle of
	*  rods per column, the columns alternating between two forms of the
	*  digits so that a lone rod can never be confused with its rotated
	*  twin.  The Unicode block encodes both forms (Supplementary
	*  Multilingual Plane, all 4 bytes of UTF-8):
	*
	*    U+1D360..U+1D368  COUNTING ROD UNIT DIGIT ONE..NINE
	*                      𝍠 𝍡 𝍢 𝍣 𝍤 𝍥 𝍦 𝍧 𝍨
	*    U+1D369..U+1D371  COUNTING ROD TENS DIGIT ONE..NINE
	*                      𝍩 𝍪 𝍫 𝍬 𝍭 𝍮 𝍯 𝍰 𝍱
	*
	*  The units column takes the unit set, the tens column the tens set,
	*  the hundreds the unit set again, and so on — the Song-dynasty
	*  orientation, which is also what the Unicode names assume.
	*
	*  The rods had NO zero — an empty column was literally left empty,
	*  and the reader counted columns to know each place — so the blank
	*  is rendered as space here, the cuneiform gap generalised to base
	*  ten: every place boundary contributes exactly one space, and a
	*  blank place contributes one of its own.  A well-formed string
	*  therefore holds exactly (places − 1) spaces, which is what makes
	*  the faithful blank round-trip:
	*
	*         𝍪 𝍢       = 23        (adjacent places: one joiner)
	*         𝍠  𝍠      = 101       (the blank tens place: joiner plus one)
	*         𝍠            = 100       (𝍠 plus two blanks, not the lone 1)
	*
	*  A column's form must match the parity of its place: writing 11 as
	*  𝍠 𝍠 instead of 𝍩 𝍠 is the rod equivalent of a DISH before
	*  a U, and pn_rods_parse rejects it.  There is no sign of negative
	*  values (the red/black rods of later Japanese counting are outside
	*  this rendering), and modern transcriptions' 〇 U+3007 is not
	*  written — a zero is a blank, as on the counting board.
	* ---------------------------------------------------------------------- */

// MARK: - Rod Numerals

#define PN_ROD_UNIT_DIGIT_ONE 0x1D360u /* …through U+1D368 (digit d: +d-1)  */
#define PN_ROD_TENS_DIGIT_ONE  0x1D369u /* …through U+1D371 (digit d: +d-1) */
#define PN_ROD_MAX_PLACES 19           /* 10^19 − 1 still fits uint64_t     */
	#define PN_ROD_MAX_VALUE 9999999999999999999ull /* 10^19 − 1: nineteen nines   */

/*  Exact number of bytes (excluding the NUL) pn_rods_render will write
	*  for a well-formed n: four per rod drawn, plus one space per place
	*  boundary — i.e. 4*(count of nonzero digits) + (count of digits − 1).
	*  Pure and total — it never fails; for n > PN_ROD_MAX_VALUE its answer
	*  is simply meaningless, since render rejects such values with
	*  pn_err_range.  render_size(0) == 0: the empty board. */
size_t pn_rods_render_size(uint64_t n);

/*  Render n in rod numerals into buf as UTF-8, NUL-terminated: one rod
	*  glyph per nonzero digit (unit set at even places, tens set at odd),
	*  most significant first; every place boundary from the top place
	*  down to the units writes exactly one space — joiner between
	*  adjacent columns, blank for empty ones (trailing blanks included).
	*  n == 0 renders the empty string.  Needs exactly
	*  pn_rods_render_size(n) bytes + NUL; a smaller buffer gives
	*  pn_err_buffer and leaves buf untouched.
	*
	*  Errors: pn_err_null, pn_err_range (n > PN_ROD_MAX_VALUE),
	*  pn_err_buffer.  On error buf is untouched. */
pn_status pn_rods_render(uint64_t n, char *buf, size_t buflen);

/*  Parse a rendered rod string — the inverse of pn_rods_render.  The
	*  string is exactly the two rod sets and spaces, in canonical
	*  spacing: it never starts with a space, holds at least one glyph,
	*  and the total count of spaces is one less than the number of
	*  places the value needs — the parser sets places = spaces + 1,
	*  reads the leftmost glyph at place places − 1, and each run of r
	*  spaces advances the place by r:
	*
	*         𝍪 𝍢       ->    23
	*         𝍠  𝍠      ->    101      (the blank tens column)
	*         𝍩             ->    10       (the blank units column)
	*
	*  Round-trip law, exact: parsing buf after a successful render
	*  recovers n.  An empty buffer (buflen == 0) parses as 0.  Errors:
	*  pn_err_null, pn_err_syntax (leading space, all spaces, two glyphs
	*  with no space between, a glyph whose set contradicts its place's
	*  parity, any other byte, a sign cut off short), pn_err_range (more
	*  than PN_ROD_MAX_PLACES places: spaces + 1 > 19).  On error *out is
	*  untouched. */
pn_status pn_rods_parse(const char *buf, size_t buflen, uint64_t *out);

/* ------------------------------------------------------------------------
  *  Indian Brahmi numerals (UTF-8 rendering).
  *
  *  Brahmi numerals (attested from the 3rd century BCE, on Ashoka's edicts
  *  and the coins of the Western Satraps) are the direct graphic ancestors
  *  of the modern Hindu-Arabic digits — but conceptually a different
  *  system: a non-positional decimal notation with NO zero.  See
  *  https://en.wikipedia.org/wiki/Brahmi_numerals .  The twenty signs, per
  *  the Unicode Standard names list (all in the Brahmi block
  *  U+11000..U+1107F, all 4 bytes of UTF-8):
  *
  *    U+11052..U+1105A  BRAHMI NUMBER ONE..NINE      𑁒 𑁓 𑁔 𑁕 𑁖 𑁗 𑁘 𑁙 𑁚
  *    U+1105B..U+11063  BRAHMI NUMBER TEN..NINETY    𑁛 𑁜 𑁝 𑁞 𑁟 𑁠 𑁡 𑁢 𑁣
  *    U+11064           BRAHMI NUMBER ONE HUNDRED    𑁤
  *    U+11065           BRAHMI NUMBER ONE THOUSAND   𑁥
  *
  *  Unlike Egypt, no digit is written by repetition: every unit 1..9 and
  *  every ten 10..90 has its own dedicated sign, so 47 needs two signs,
  *  not eleven.  There is NO sign above the thousand (the Jain infinities
  *  counted in words, not signs) and NO zero — a lone blank is the
  *  nothing owed — so the renderable domain is 0..PN_BRAHMI_MAX_VALUE
  *  and zero renders as the empty string, the scribe's blank again.
  *
  *  Hundreds and thousands are MULTIPLICATIVE, per Unicode 14.x: a digit
  *  2..9 ligatured with the power sign — the power first, then the
  *  U+1107F BRAHMI NUMBER JOINER, then the multiplier — so 400 is
  *  <𑁤 𑁿 𑁕> (100 JOINER 4) rendered as one stacked ligature, while an
  *  additive component stays unligated: 104 is <𑁤 𑁕> (100 4).  The joiner
  *  marks that semantic distinction, so it is never dropped, never
  *  doubled, and legal only between number signs — 1 × 100 is written
  *  with the bare power sign, as 𑁥𑁥 is no number any more than 𓏤..𓏤 ten
  *  strokes were.
  *
  *  Canonical form: up to four terms descending — thousands, hundreds,
  *  tens, ones — each at most once, with no separators; a signs table
  *  and the four slots make every string self-delimiting:
  *
  *    𑁤𑁿𑁓𑁟𑁗  = 256   (the Sasaram edict: 200 ligature + fifty + six)
  *    𑁤𑁟𑁔         = 153   (the Damasena coin: 100-50-3)
  *    𑁥𑁿𑁚𑁤𑁿𑁚𑁣𑁚 = 9999  (the full set; 𑁚 appears three times, in three
  *                          slots — slots, not sign identity, rule)
  * ---------------------------------------------------------------------- */

// MARK: - Brahmi Numerals

#define PN_BRAHMI_NUMBER_ONE       0x11052u /* 𑁒 …through U+1105A NINE  = 1..9 */
#define PN_BRAHMI_NUMBER_TEN         0x1105Bu /* 𑁛 …through U+11063 NINETY = 10..90 */
#define PN_BRAHMI_NUMBER_HUNDRED     0x11064u /* 𑁤 one hundred                 */
#define PN_BRAHMI_NUMBER_THOUSAND    0x11065u /* 𑁥 one thousand                */
#define PN_BRAHMI_NUMBER_JOINER      0x1107Fu /* 𑁿 ligature join, numbers only */
#define PN_BRAHMI_MAX_VALUE          9999u    /* four filled slots: no sign above 𑁥 */

/*  Exact number of bytes (excluding the NUL) pn_brahmi_render will
  *  write for a well-formed n: four per sign drawn, joiner included —
  *  a thousands or hundreds term costs four bytes with its digit 1
  *  (bare power sign) and twelve with its digit 2..9 (power, joiner,
  *  multiplier), a tens or ones term four bytes when present.  Pure and
  *  total — it never fails; for n > PN_BRAHMI_MAX_VALUE its answer is
  *  simply meaningless, since render rejects such values with
  *  pn_err_range.  render_size(0) == 0: the blank. */
size_t pn_brahmi_render_size(uint64_t n);

/*  Render n in Brahmi numerals into buf as UTF-8, NUL-terminated: the
  *  terms descending thousands, hundreds, tens, ones; each nonzero digit
  *  its dedicated sign (1..9, or TEN..NINETY for the tens digit), and a
  *  hundreds or thousands digit d written ligatured: d == 1 the bare
  *  power sign, d == 2..9 the power sign, then U+1107F, then NUMBER_d.
  *  n == 0 renders the empty string — the blank.  Needs exactly
  *  pn_brahmi_render_size(n) bytes + NUL; a smaller buffer gives
  *  pn_err_buffer and leaves buf untouched.
  *
  *  Errors: pn_err_null, pn_err_range (n > PN_BRAHMI_MAX_VALUE),
  *  pn_err_buffer.  On error buf is untouched. */
pn_status pn_brahmi_render(uint64_t n, char *buf, size_t buflen);

/*  Parse a rendered Brahmi string — the inverse of pn_brahmi_render.
  *  Strict canonical form: the twenty number signs and the joiner, in
  *  at most four descending terms — thousands, hundreds, tens, ones —
  *  each term at most once, no separators:
  *
  *      𑁤𑁿𑁓𑁟𑁗  ->  256     (𑁤 𑁿 𑁓 = 200 ligatured, 𑁟 = 50, 𑁗 = 6)
  *      𑁤𑁕              ->  104     (additive component: 100 then 4)
  *      𑁥𑁒              ->  1001    (thousand then a bare one)
  *
  *  Round-trip law, exact: parsing buf after a successful render
  *  recovers n.  An empty buffer (buflen == 0) parses as 0 — the blank
  *  reads back as the nothing it was.  The grammar admits no value past
  *  PN_BRAHMI_MAX_VALUE, so overflow needs no check.  Violations are
  *  pn_err_syntax the way 𓏤.. bottom-up was: a second term of one
  *  slot, an ascending term, a joiner doubled or leading or trailing or
  *  joining anything but a multiplier after its power sign, a ligatured
  *  multiplier of 1 (𑁥𑁿𑁒 — write 𑁥 bare), a ligature left unligated
  *  (𑁤𑁕𑁕 would read 100+4+4: no slot allows the second 𑁕), any other
  *  byte, a sign cut off short.  On error *out is untouched.
  *
  *  Errors: pn_err_null, pn_err_syntax. */
pn_status pn_brahmi_parse(const char *buf, size_t buflen, uint64_t *out);

/* ------------------------------------------------------------------------
 *  Mayan bar-and-dot numerals over pn_vigesimal — the renderer.
 *
 *  The Maya had what no other ancient place-value script here can
 *  claim, and what Unicode's cuneiform block pointedly lacks: a real
 *  zero.  Dots for units, bars for fives, a shell for nought, and the
 *  block composites them into one sign per place 0..19:
 *
 *  U+1D2E0..U+1D2E4   MAYAN NUMERAL ZERO..FOUR      shell, dots   = 0..4
 *  U+1D2E5..U+1D2E9   MAYAN NUMERAL FIVE..NINE      bars + dots   = 5..9
 *  U+1D2EA..U+1D2EE   MAYAN NUMERAL TEN..FOURTEEN   two bars      = 10..14
 *  U+1D2EF..U+1D2F3   MAYAN NUMERAL FIFTEEN..NINETEEN three bars  = 15..19
 *
 *  (Unicode block Mayan Numerals, U+1D2E0..U+1D2F3; Unicode 10.0, 2017
 *  after proposal L2/16-264, Jameson Quinn.  All twenty are SMP: four
 *  bytes of UTF-8 each under the stem F0 9D 8B.)
 *
 *  The codex stacks places vertically, least significant at the
 *  bottom; we transcribe the stack top-down made left-to-right —
 *  most significant place first, one space per place boundary, exactly
 *  the linearisation the rod columns get:
 *
\t *      𝋡 𝋣   ->   23     (three dots under one dot: 1x20 + 3)
\t *      𝋡 𝋭   ->   33     (dot above three dots atop two bars)
\t *      𝋡 𝋡 𝋩   ->   429     (dot dot nine: 1x400 + 1x20 + 9)
\t *      𝋡 𝋢 𝋠 𝋠   ->   8800     (the shells hold rows three and four)
\t *      𝋠   ->   0     (the shell itself: no blank needed)
 *
 *  Each place carries exactly ONE precomposed sign, so a zero place
 *  writes its shell instead of the Babylonian's two-space gap: the
 *  string is self-counting, no context tells a reader where the rows
 *  are.  pn_mayan_render draws every place that well_formed allows,
 *  including fractional ones (no radix point is carved; the '-' is an
 *  anachronism, as in the cuneiform renderer above), and pn_mayan_parse
 *  answers period style: radix == n_places.
 *
 *  References: https://en.wikipedia.org/wiki/Maya_numerals ,
 *  https://en.wikipedia.org/wiki/Vigesimal
 * ------------------------------------------------------------------------ */

// MARK: - Mayan Numerals

#define PN_MAYAN_DIGIT_ZERO 0x1D2E0u /* SHELL …through U+1D2F3 (digit d: +d) */

/*  Bytes needed to render v as bar-and-dot Maya numerals, NOT counting
 *  the terminating NUL: one sign per place and one space per place
 *  boundary, plus a '-' when neg.  Pure and total — it never fails; on
 *  a malformed v its answer is simply meaningless, since render rejects
 *  such values with a PN_ERROR_DOMAIN note. */
size_t pn_mayan_render_size(pn_vigesimal v);

/// Write @param v as Maya bar-and-dot numerals into @param buf as
/// UTF-8, ending with a NUL: one sign per place, one space per
/// boundary.  A zero place writes the shell, not a gap.
/// @param v - the value to render
/// @param buf - where the signs land, with room for the NUL
/// @param buflen - bytes of room at buf
/// @param error - where to hang the note on failure: PN_ERROR_NULL
///     when buf is NULL, PN_ERROR_DOMAIN when v is malformed
///     (n_places == 0 or past the max, a place above 19, radix past
///     n_places), PN_ERROR_BUFFER when buflen does not beat
///     pn_mayan_render_size(v).  On any failure buf is left
///     untouched.
/// @returns true on success, false with a note in @param error.
bool pn_mayan_render(pn_vigesimal v, char *_Nonnull buf, size_t buflen,
                     pn_error *_Nullable *_Nullable error);

/// Read back a rendered bar-and-dot string — the exact inverse of
/// pn_mayan_render.   ,@param buf holds @param buflen bytes of UTF-8
/// using '-', the twenty signs, and single joining spaces; the
/// grammar is '-? (sign ' ')* sign' and it is strict: a leading,
/// trailing or doubled space, two signs touching inside one row, or
/// any foreign byte (the block's void past U+1D2F3 included —
/// U+1D2F4..1D2FF carry no sign and never parse), or a sign cut off
/// in the middle of its UTF-8 is PN_ERROR_SYNTAX.  More rows than
/// PN_VIGESIMAL_MAX_PLACES is the page overrun — PN_ERROR_RANGE, the
/// ruling the scribe's fifteenth row gets — not bad grammar.  An
/// empty string (buflen == 0) is syntax, not zero: the shell is
/// zero's one true spelling, the way an empty tablet is no number.
/// The round trip law holds: parsing buf after a successful render
/// recovers v's places exactly (the parsed radix == n_places: period
/// style — a carved stack carries no point a reader could place
/// back).
/// @param buf - the string to read
/// @param buflen - bytes to read at buf
/// @param out - where the value lands on success; untouched on any
///     failure
/// @param error - where to hang the note on failure.
/// @returns true on success, false with a note in @param error.
bool pn_mayan_parse(const char *_Nullable buf, size_t buflen,
                    pn_vigesimal *_Nullable out,
                    pn_error *_Nullable *_Nullable error);

/* ------------------------------------------------------------------------
 *  Inca quipu — positional decimal knot notation, transcribed in UTF-8.
 *
 *  The quipucamayuq tied numbers onto pendant cords hanging from a
 *  main cord: base ten by POSITION along the cord, the units line
 *  nearest the free end and each higher place marching up toward the
 *  main cord (L. Leland Locke established the place value in 1912;
 *  Ascher & Ascher read the archives).  Our transcription lays the
 *  pendant down, most significant place first — the linearisation the
 *  counting-board rods get in rods.c:
 *
 *  Unicode encodes NO quipu signs — there is no knot block to borrow,
 *  unlike the cuneiform wedges of babylonian.c — so the cord is
 *  transcribed with two generic glyphs:
 *
 *  U+2022 •  BULLET     one simple knot          (Locke's s): d of
 *                                   them mark digit d in every line
 *                                   above the units
 *  U+221E ∞  INFINITY   the figure-eight knot (Locke's E) at the
 *                                   units line, and d of them in a row
 *                                   the long knot of d turns (d L)
 *
 *  Both are BMP: three bytes of UTF-8 each, stems E2 80 A2 and
 *  E2 88 9E.  In Locke's own shorthand, the article's worked cords:
 *
 *        ••••••• ••• ∞      ->      731      (7s, 3s, E)
 *        ••••••••  ∞∞∞∞      ->      804      (8s, X, 4L: the blank tens keeps two boundaries)
 *        • •••• ••••••••• ∞∞∞      ->      1493      (1s, 4s, 9s, 3L)
 *        •       ->      10      (one knot over the blank units)
 *        ∞      ->      1      (figure-eight: units never say bullet)
 *              ->      0      (the empty cord: no knot tied)
 *
 *  The Inca zero was the ABSENCE of a knot in the right position — the
 *  truest blank in this library, kept literally: one space per place
 *  boundary and a blank place writes no sign of its own, so a
 *  well-formed cord holds exactly (places − 1) spaces and the parser
 *  reads places = spaces + 1 off the cord unaided.  The figure-eight
 *  is the reader's anchor — it lives at the units line and nowhere
 *  else, as good as the distinctive units knot on the real cord — so
 *  ∞ above the units line is no cord, and a lone simple knot is no
 *  number (the tens digit of 10 hangs over the blank units with its
 *  boundary space kept).
 *
 *  One cord, one number: color, ply, S/Z twist, subsidiary cords and
 *  the chroniclers' multi-number pendants are not the number system,
 *  and no transcription pretends at them.
 *
 *  References: https://en.wikipedia.org/wiki/Quipu ,
 *  Locke, The Ancient Quipu or Peruvian Knot Record (1912);
 *  Ascher & Ascher, Mathematics of the Incas: Code of the Quipu (1997).
 * ------------------------------------------------------------------------ */

// MARK: - Quipu Numerals

#define PN_QUIPU_SIMPLE_KNOT     0x2022u   /*   BULLET  UTF-8: E2 80 A2         */
#define PN_QUIPU_LONG_KNOT       0x221Eu   /* ∞ INFINITY UTF-8: E2 88 9E         */
#define PN_QUIPU_MAX_PLACES      19        /* 10^19 − 1 still fits uint64_t     */
#define PN_QUIPU_MAX_VALUE 9999999999999999999ull /* nineteen nines               */

/*  Bytes needed to render n as a quipu cord, NOT counting the
 *  terminating NUL.  Pure and total — it never fails; for n >
 *  PN_QUIPU_MAX_VALUE its answer simply runs on and is meaningless,
 *  since render rejects such n with pn_err_range. */
size_t pn_quipu_render_size(uint64_t n);

/*  Render n (0 .. PN_QUIPU_MAX_VALUE) as a quipu cord into buf as
 *  UTF-8, NUL-terminated.  One column of knots per place: d bullets
 *  above the units line, d infinities AT it (the long knot's turns,
 *  the figure-eight its first); a blank place writes no knots but
 *  keeps its boundary space, and 0 renders the empty cord — the Inca
 *  zero is no knot tied.
 *
 *  Errors: pn_err_null, pn_err_range (n > PN_QUIPU_MAX_VALUE: a
 *  twentieth line no pendant holds), pn_err_buffer (buflen <=
 *  render_size(n)).  On error buf is untouched. */
pn_status pn_quipu_render(uint64_t n, char *buf, size_t buflen);

/*  Parse a rendered cord — the inverse of pn_quipu_render.  Strict
 *  canonical form: places = spaces + 1, the leftmost column sitting at
 *  place places − 1 and each space advancing one place; every column a
 *  run of one to nine IDENTICAL knots — bullets in the lines above the
 *  units, infinities at the units line alone (the figure-eight is
 *  where the cord ends).  A lone bullet at the units line, ∞
 *  anywhere above it, a mixed column, a leading or trailing space,
 *  columns touching with no boundary, any foreign byte (the
 *  chronicler's minus included), a knot cut off mid-UTF-8 — all
 *  pn_err_syntax; ten identical knots in one column, or more than
 *  PN_QUIPU_MAX_PLACES places, is more turns and more cord than a
 *  digit and a uint64_t hold: pn_err_range, the ruling the scribe's
 *  twentieth row gets.  An empty cord (buflen == 0) parses as 0 — the
 *  blank reads back as the nothing no knot was.  On error *out is
 *  untouched. */
pn_status pn_quipu_parse(const char *buf, size_t buflen, uint64_t *out);

/* ------------------------------------------------------------------------
 *  Greek alphabetic numerals — the Milesian (Ionic) system, in UTF-8.
 *
 *  No place values, no zero: the market and the chancery took the
 *  alphabet itself as the digit set — the first three letters of each
 *  row are its unit: α (1), ι (10), ρ (100) — and filled the gaps with
 *  three letters the classical alphabet had lost but the numerals
 *  kept: ϛ stigma (6), ϟ koppa (90), ϡ sampi (900).  Above 999 the
 *  thousands get their own run of the same signs, announced by the
 *  low keraia ͵ (U+0375) in front of them, and the whole number wears
 *  the trailing keraia ʹ (U+0374) behind it — the horn-mark of the
 *  stenographers, a keraia being to Greek numerals what the decimal
 *  point is to ours: the reader's proof that a number is being read.
 *  So every nonempty rendering carries exactly one ʹ at the end, and
 *  the parser demands it where the culture did.
 *
 *  A spelling is a descending SCALE RUN — at most one sign from the
 *  hundreds row ρ σ τ υ φ χ ψ ω ϡ, then the tens row ι κ λ μ ν ξ ο π
 *  ϟ, then the units row α β γ δ ε ϛ ζ η θ — nothing backward, no
 *  sign twice: 999 is never ροο and 150 is never νρ, for exactly the
 *  reason no scribe stacked nine 𓏥 coils.  A legal run is the ONLY
 *  run spelling its value: the rows are worth 100/10/1, so a run is
 *  decimal notation in letters and the descent is forced.
 *
 *  Above 999 the cord is two runs under one low keraia — thousands
 *  first, then units — and the culture let them butt together with
 *  nothing between: ͵ιαʹ then reads BOTH 11000 (ια = 11 thousands)
 *  and 10001 (ι = 10 thousands, α = 1 unit); an exhaustive sweep of
 *  the domain counts 972 of those shared cords, so a number whose
 *  thousands run AND units run are both live separates them with ONE
 *  space (the stenographers' own gutter): 10001 renders ͵ι␣αʹ and
 *  11000 renders ͵ιαʹ, and the space, like the quipu's, is part of
 *  the canonical cord.  Whole thousands carry no space (nothing to
 *  separate); numbers below 1000 carry no low keraia at all.  Two
 *  runs may share a letter — 1001 is ͵α␣αʹ — the space, not
 *  uniqueness of letters, is what tells the runs apart.  The
 *  article's table, worked through this transcription:
 *
 *    αʹ           ->    1
 *    ιθʹ          ->    19
 *    σνϛʹ         ->    256
 *    χϛʹ          ->    606
 *    ϡϟθʹ         ->    999
 *    ͵αʹ          ->    1000
 *    ͵α␣αʹ        ->    1001
 *    ͵β␣ιθʹ       ->    2019
 *    ͵ι␣αʹ        ->    10001
 *    ͵ιαʹ         ->    11000
 *    ͵ϡϟθ␣ϡϟθʹ    ->    999999
 *
 *   (999999 = 999 thousands over 999 units — the culture wrote no
 *  millions in these signs, and PN_GREEK_MAX_VALUE stops where the
 *  alphabet does.)
 *
 *  The majuscule of printed editions (͵Β␣ΙΘʹ for 2019) is typography,
 *  not notation: this module renders and parses lowercase only, and
 *  capitals are pn_err_syntax, like every foreign letter — Roman I V
 *  X among them (the Roman numerals keep their own block U+2160.. and
 *  will get their own module).
 *
 *  One trap before normalising these cords: the trailing keraia
 *  U+0374 is NFC-UNSTABLE — NFD unfolds it to U+02B9 MODIFIER LETTER
 *  PRIME and NFC never brings it back (every cord of this module,
 *  1..999999, falls to that trap; the low keraia U+0375 is stable).
 *  The module writes the canonical CD B4 bytes and accepts nothing
 *  else: the prime, the tonos ΄ U+0384 and the dakia are near-miss
 *  impostors — pn_err_syntax, like the Mayan dot on a quipu cord.
 *
 *  6/90/900 take the archaic ϛ ϟ ϡ (U+03DB, U+03DF, U+03E1, Greek-and-
 *  Coptic extensions, NFC-stable); 200 takes plain σ U+03C3 — the
 *  lunate ϲ of Byzantine hands and the final ς of living Greek are
 *  BOTH refused, the final sigma being a sandhi rule of the language,
 *  not a numeral sign, and ϲ U+03F2 colliding with san.
 *
 *  Zero had no glyph (the culture counted what was there); like the
 *  Egyptian scribe's blank, the rods' empty column and the unstrung
 *  quipu cord, n == 0 renders the empty string and buflen == 0 reads
 *  back as 0 — this section follows the uint64-direct rule book.
 *
 *  References: https://en.wikipedia.org/wiki/Greek_numerals ,
 *  https://en.wikipedia.org/wiki/Keraia
 * ------------------------------------------------------------------------ */

// MARK: - Greek Numerals

#define PN_GREEK_MAX_VALUE 999999ull /* two full runs, 999 of 1000 over
                                        999 — written millions needed
                                        fresh marks no standard survived */

/*  Exact number of bytes (excluding the NUL) pn_greek_render will
 *  write: TWO per sign — every sign is a 2-byte BMP code point, both
 *  keraia included — plus ONE for the run-separating space when both
 *  runs are live (n >= 1000 with units above it).  n == 0 renders the
 *  empty cord. */
size_t pn_greek_render_size(uint64_t n);

/*  Render n (0 .. PN_GREEK_MAX_VALUE) as a Milesian numeral: the low
 *  keraia ͵ then the thousands run when n >= 1000, then — when both
 *  runs are live — the separating space, then the units run, then the
 *  trailing keraia ʹ; every run descending, no sign twice within a
 *  run, NUL-terminated.  Needs pn_greek_render_size(n) + 1 bytes.
 *  Errors: pn_err_null, pn_err_range (n > PN_GREEK_MAX_VALUE),
 *  pn_err_buffer.  On error buf is untouched. */
pn_status pn_greek_render(uint64_t n, char *buf, size_t buflen);

/*  Parse a rendered numeral — the exact inverse, verified bijective
 *  over the whole domain.  Strict canonical form: the cord ends in
 *  the trailing keraia (its canonical CD B4 bytes; the NFC twin
 *  U+02B9 is a foreign byte) and nowhere else; at most one low keraia
 *  leads the cord and it may appear nowhere else; between them the
 *  signs split on at most ONE space into two runs, each a legal
 *  descending run — one sign per row, rows H > T > U, never backward,
 *  no sign twice; the space is legal only between two live runs
 *  (none touching either end, none doubled) and its left run counts
 *  thousands, its right run units; a cord WITHOUT the space and
 *  WITHOUT the low keraia is a bare units run below 1000; with the
 *  low keraia and no space it is whole thousands.  All else is
 *  pn_err_syntax: doubled sign within a run, backward pair (θ before
 *  ι, α before ρ when no space or row-change separates them),
 *  capitals, the lunate ϲ, the final ς, the digamma ϝ, the tonos,
 *  U+02B9, Latin and Roman letters, any cut-off 2-byte sequence.  A
 *  low keraia with no run under it and a lone trailing keraia are no
 *  number: pn_err_syntax.  No legal cord exceeds PN_GREEK_MAX_VALUE,
 *  so overflow needs no table; like the other blanks it honors,
 *  buflen == 0 parses as 0.  On error *out is untouched. */
pn_status pn_greek_parse(const char *buf, size_t buflen, uint64_t *out);

/* ------------------------------------------------------------------------
 *  Roman numerals — the canonical form, in ordinary ASCII letters.
 *
 *  No zero, no place values: the ledger kept seven capital letters —
 *   I (1), V (5), X (10), L (50), C (100), D (500), M (1000) — and the
 *  subtractive habits of the Middle Ages: a unit written BEFORE the
 *  sign it undershoots stands for the gap — IV is 5 less 1, IX is 10
 *  less 1.  The grammar this module renders and reads is the tallied
 *  canonical form — the per-digit rows of thousands, hundreds, tens
 *  and units, always in full: 3888 is MMMDCCCLXXXVIII, the longest
 *  numeral there is, and 1999 is MCMXCIX — never the shortcuts the
 *  clocks and ink allowed (IIII for 4, IC for 99, VX, and the
 *  overlined vincula that meant 5000 would be other cultures'
 *  invention; the abacist editors settled on these letters alone).
 *
 *  A canonical numeral is the ONLY spelling of its value: the rows
 *  descend, each run of I, X, C or M holds at most three, the six
 *  subtractive digraphs IV IX XL XC CD CM appear at most once and
 *  belong to their digit — that descent is forced, the same way the
 *  Greek run's is, so a numeral reads back exactly one number.  The
 *  culture's own proofs agree: the same year is MCMXCIX in every
 *  inscription that spells it full.
 *
 *  The article's table, worked through this transcription:
 *
 *    I                ->   1
 *    IV               ->   4
 *    V                ->   5
 *    IX               ->   9
 *    XIV              ->   14
 *    XLIV             ->   44
 *    XC               ->   90
 *    CD               ->   400
 *    CM               ->   900
 *    M                ->   1000
 *    MCMXCIX          ->   1999
 *    MMMDCCCLXXXVIII  ->   3888
 *    MMMCMXCIX        ->   3999
 *
 *   (3999 = MMM | CM | XC | IX, the ceiling; the Romans wrote larger
 *  with overlines and fresh marks no standard survived.)
 *
 *  Zero had no letter (the ledger counted what was there — Bede's
 *  abbey later wrote nulla, the nothing, in its place); like the
 *  Egyptian blank, the rods' gap, the unstrung cord and the silent
 *  keraia, n == 0 renders the empty string and buflen == 0 reads
 *  back as 0 — this section follows the uint64-direct rule book.
 *
 *  Lowercase letters, Greek, whacked-out spaced variants and every
 *  foreign byte are pn_err_syntax — a numeral in the abacist's sense
 *  is capital ASCII letters only.
 *
 *  References: https://en.wikipedia.org/wiki/Roman_numerals
 * ------------------------------------------------------------------------ */

// MARK: - Roman Numerals

#define PN_ROMAN_MAX_VALUE 3999ull /* MMM CM XC IX: three thousands and
                                      the alphabet's full hundreds-tens-
                                      units row — vinculum numbers had no
                                      standard to keep them */

#define PN_ROMAN_FORMAT_MAX_LEN 16 /* 15 letters — the canon of 3888 —
                                      plus the NUL                   */

/*  Exact number of bytes (excluding the NUL) pn_roman_render will
 *  write: the letters of the four per-digit rows, at most four per
 *  live row (DCCC, XXX, VIII...).  n == 0 renders the empty cord. */
size_t pn_roman_render_size(uint64_t n);

/*  Render n (0 .. PN_ROMAN_MAX_VALUE) in the canonical tallied form:
 *  the thousands run, the hundreds row, the tens row, the units row,
 *  NUL-terminated.  Needs pn_roman_render_size(n) + 1 bytes.
 *  Errors: pn_err_null, pn_err_range (n > PN_ROMAN_MAX_VALUE),
 *  pn_err_buffer.  On error buf is untouched. */
pn_status pn_roman_render(uint64_t n, char *buf, size_t buflen);

/*  Parse a canonical numeral — the exact inverse, bijective over the
 *  domain.  Strict canonical form: runs of I, X, C, M at most three;
 *  each subtractive digraph (IV, IX, XL, XC, CD, CM) at most once and
 *  only as its digit's spelling; the tokens in descending value.
 *  Anything else is pn_err_syntax: a fourth I/X/C/M (IIII, the clock
 *  maker's heresy), a subtractive digraph out of its digit (IL, IC,
 *  VX, XM), tokens out of order (IX before V), a lowercase letter,
 *  a space, a NUL, any non-ASCII byte.  The empty cord is Bede's
 *  nulla: buflen == 0 parses as 0.  On error *out is untouched. */
pn_status pn_roman_parse(const char *buf, size_t buflen, uint64_t *out);

/* --- early mathematical theory (Elements VII-IX, the sieve of
     Eratosthenes, the gnomon figures) ---------------------------------
 *
 *  The reckoning that sat underneath the numerals: Euclid's ladder
 *  of remainders, the papyrus of struck rows, the Pythagoreans'
 *  triples, the abacist's ledger of factors.  Nothing here renders a
 *  notation — these are the working functions of the culture, on the
 *  whole uint64 ledger 0..18446744073709551615.
 *
 *  Primality is proved, never merely probable: below 2^32 Euclid's
 *  own trial division by the sieve's unstruck rows is the complete
 *  argument, and above it Miller's seven certified witnesses (2,
 *  325, 9375, 28178, 450775, 9780504, 1795265022) decide every case
 *  of the ledger before the water clock runs out.
 */

// MARK: - Math

#define PN_MATH_SIEVE_MAX_VALUE         1000000ull
#define PN_MATH_M_MAX_VALUE             65535ull
#define PN_MATH_SIDE_MAX_VALUE          4294967295ull
#define PN_MATH_MAX_FACTORS             64u

/*  The papyrus of Eratosthenes.  pn_math_sieve_size answers pi(limit),
 *  the count of primes from 2 to limit; it answers 0 when limit < 2
 *  or passes PN_MATH_SIEVE_MAX_VALUE = 1000000, the last row of the
 *  papyrus.  pi(100) = 25, pi(1000) = 168, pi(1000000) =
 *  78498.  pn_math_sieve lays the board in ascending order and
 *  reports the rows it set in *count: sieve(30) ends at 29,
 *  sieve(1000000) at 999983.  Errors like the other ledgers:
 *  pn_err_null, pn_err_range, pn_err_buffer (cap short of the count;
 *  the caller's rows and *count keep their word). */
size_t pn_math_sieve_size(uint64_t limit);

pn_status pn_math_sieve(uint64_t limit, uint64_t *out, size_t cap,
                        size_t *count);

/*  Euclid's ladder (Elements VII.1-2): the greatest common measure.
 *  gcd(1071, 462) = 21; gcd(0, 0) = 0 by the abacist's
 *  convention.  lcm(1071, 462) = 23562; a blank row makes the ratio
 *  blank: lcm(0, n) = 0.  A product past the ledger top is
 *  pn_err_range.  Both answer pn_err_null on a NULL out. */
pn_status pn_math_gcd(uint64_t a, uint64_t b, uint64_t *out);

pn_status pn_math_lcm(uint64_t a, uint64_t b, uint64_t *out);

/*  The Pythagoreans' gnomon figures.  is_square(999950884) is true —
 *  the square of 31622.  is_prime(561) answers false: Siegel's
 *  number, the Carmichael fool that fools the Fermat test alone.
 *  next_prime is inclusive, the culture counting the number itself
 *  when it is already prime: next_prime(1) = 2, next_prime(2) =
 *  2, next_prime(46340) = 46349; past the last prime of the
 *  ledger it answers pn_err_range. */
pn_status pn_math_is_square(uint64_t n, bool *out);

pn_status pn_math_is_prime(uint64_t n, bool *out);

pn_status pn_math_next_prime(uint64_t n, uint64_t *out);

/*  The abacist's ledger of factors: n = f[0] x f[1] x ... ascending,
 *  repeats kept — the culture wrote 90 as 2 x 3 x 3 x 5 and 649 as 11 x 59.
 *  The blank and the unit are the empty product:
 *  factorize_size(0) = factorize_size(1) = 0.  The longest braid on
 *  the ledger is 63 twos (2^63), so PN_MATH_MAX_FACTORS = 64 rows
 *  always hold.  A cap short of the count is pn_err_buffer; on any
 *  error the caller's rows and *count keep their word. */
size_t pn_math_factorize_size(uint64_t n);

pn_status pn_math_factorize(uint64_t n, uint64_t *out, size_t cap,
                            size_t *count);

/*  The numbers equal to their own parts (Elements IX.36): 6 = 1 + 2
 *  + 3, 28, 496, 8128 — the even form Euclid built and Euler proved
 *  necessary.  The culture weighed proper divisors from 1 up, so the
 *  blank and the unit sit out of the domain: perfect_check(0) is
 *  pn_err_range, while perfect_check(90) answers pn_ok with false. */
pn_status pn_math_perfect_check(uint64_t n, bool *out);

// MARK: - Pythagoreas

/*  The Plimpton tablet's loom (Elements X.29): from m > n >= 1 with
 *  m <= PN_MATH_M_MAX_VALUE = 65535, the legs are m*m - n*n and
 *  2*m*n and the hypotenuse m*m + n*n.  m = 2, n = 1 weaves 3,
 *  4, 5; m = 12, n = 5 weaves 119, 120, 169.
 *  Any side past PN_MATH_SIDE_MAX_VALUE = 4294967295 (the rod's own
 *  foot) is pn_err_range.  pythagorean_check weighs the rod: (119,
 *  120, 169) stands; sides out of the rod's foot are pn_err_range. */
pn_status pn_math_pythagorean_triple(uint64_t m, uint64_t n,
                                     uint64_t *leg1, uint64_t *leg2,
                                     uint64_t *hypotenuse);

pn_status pn_math_pythagorean_check(uint64_t x, uint64_t y, uint64_t z,
                                    bool *out);

#ifdef __cplusplus
}
#endif
#endif /* PANTHEON_H */
