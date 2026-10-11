# libPantheon
The road to `math.h`

## Goals
Encapsulate functions from the [history of mathematics](https://en.wikipedia.org/wiki/History_of_mathematics) and science in well documented, easy to understand implementations that are efficient enough for demanding applications:
- small types passed by value
- straight-line code with simple, bounded loops
- no heap allocation, except when the caller asks for a new string (the `_format` functions)
- clear error reports for every function that can fail

## Audience
libPantheon is meant for teaching: it covers fundamental mathematics in the context of software engineering.
Code documentation is informative rather than fully normative, and it includes references for further reading.
All documentation is written at a 9th grade (US) reading level.

## Using it
```
make          # builds build/libpantheon.a and the test binaries
make test     # runs every test
```

Consume the library with `-Iinclude build/libpantheon.a` and a single
`#include "pantheon.h"`.

A function that can fail takes a last argument `pn_error** error`. Set
your own `pn_error*` to `NULL` and pass its address; after a failure it
points to a report saying what went wrong, which you free with
`pn_error_free`. Functions that can't fail take no error argument.

## Number systems

These types hold numbers exactly, in the bases and forms used by
ancient mathematicians. Each one is a small struct passed by value.

### Base 60 (sexagesimal): `pn_base60`
Babylonian mathematicians counted in base 60, and we still do for time
and angles. A `pn_base60` stores up to 16 base-60 digits ("places",
each 0 to 59) plus a radix point and a sign. Because 60 divides evenly
by 2, 3, 4, 5 and 6, fractions like 1/3 (written 0;20) are exact.

- `pn_base60_parse` and `pn_base60_format` / `pn_base60_format_into`
  read and write the modern scholarly notation: the tablet YBC 7289's
  square root of 2 is `1;24,51,10`
- `pn_base60_from_u64`, `pn_base60_to_u64`, `pn_base60_from_double`
  and `pn_base60_to_double` convert to and from ordinary numbers

Further reading: [Sexagesimal](https://en.wikipedia.org/wiki/Sexagesimal), [YBC 7289](https://en.wikipedia.org/wiki/YBC_7289)

### Base 20 (vigesimal): `pn_base20`
The Maya counted in base 20. A `pn_base20` works like `pn_base60` with
places from 0 to 19, so `1;12,9` is 1 + 12/20 + 9/400 = 1.6225.

- `pn_base20_parse` and `pn_base20_format` / `pn_base20_format_into`
- `pn_base20_from_u64`, `pn_base20_to_u64`, `pn_base20_from_double`
  and `pn_base20_to_double`

Further reading: [Vigesimal](https://en.wikipedia.org/wiki/Vigesimal)

### Fractions: `pn_fraction`
A whole-number numerator over a whole-number denominator, always in
lowest terms. Fractions hold values no place-value system can write
exactly, like 1/7 in base 60 or 1/3 in binary.

- `pn_fraction_from_double` finds the closest fraction with a
  denominator no bigger than a limit you choose, using continued
  fractions: pi gives Archimedes' 22⁄7 with a limit of 10, and
  Zu Chongzhi's 355⁄113 with a limit of 1000
- `pn_fraction_format` writes `355⁄113` with ⁄ U+2044 FRACTION SLASH;
  `pn_fraction_parse` also reads ∕ U+2215, ／ U+FF0F and plain `/`
- `pn_fraction_make`, `pn_fraction_equals` and `pn_fraction_to_double`

Further reading: [Continued fraction](https://en.wikipedia.org/wiki/Continued_fraction)

## Numerals

Each numeral module writes a number in the signs of its culture, as
UTF-8 text, and reads that text back. They share one pattern:

- `pn_<name>_render(n, buf, buflen, &error)` writes `n` into your buffer
- `pn_<name>_render_size(n)` says how many bytes that takes
- `pn_<name>_parse(text, &error)` reads null terminated text back

Every module accepts only its standard spelling, so reading back what
it wrote always gives the same number. Below, ␣ marks a space that is
part of the numeral.

### Fonts

Most of these signs live outside the characters every computer ships
with. If you see empty boxes instead of signs, install a font that
covers that script. All of these are free.

| Script | Signs | Font |
|:--|:--|:--|
| Babylonian cuneiform | 𒁹 𒌋 | [Noto Sans Cuneiform](https://notofonts.github.io/) |
| Egyptian hieroglyphs | 𓏤 𓎆 𓍢 | [Noto Sans Egyptian Hieroglyphs](https://notofonts.github.io/) |
| Brahmi | 𑁒 𑁛 𑁤 𑁿 | [Noto Sans Brahmi](https://notofonts.github.io/) |
| Maya | 𝋠 𝋡 𝋩 | [BabelStone Mayan Numerals](https://babelstone.co.uk/Fonts/Mayan.html), or [Noto Sans Symbols 2](https://notofonts.github.io/) |
| Counting rods | 𝍠 𝍩 | [Noto Sans Symbols 2](https://notofonts.github.io/) |

Chinese, Greek, Roman and quipu signs (• ∞) are covered by the fonts
that come with most systems. [GNU Unifont](https://unifoundry.com/unifont/)
is a fallback that covers most of Unicode, in a simple pixel
style.

### Babylonian cuneiform: `pn_babylonian`
Base 60 pressed into clay with two wedges, 𒁹 (1) and 𒌋 (10). Each
place is some tens wedges then some ones wedges. There was no zero for
most of Babylonian history: an empty place is a gap, so a lone 𒁹 could
mean 1, 60 or 1/60. Renders and parses `pn_base60` values.

8583 = `𒁹𒁹␣𒌋𒌋𒁹𒁹𒁹␣𒁹𒁹𒁹` (places 2, 23, 3) ·
[Babylonian numerals](https://en.wikipedia.org/wiki/Babylonian_numerals)

### Egyptian hieroglyphs: `pn_egyptian`
One sign for each power of ten, repeated as needed: 𓏤 1, 𓎆 10,
𓍢 100, 𓆼 1000, 𓂭 10 000, 𓆐 100 000, 𓁨 1 000 000. Decimal but
additive, with no zero. 0 to 9 999 999.

4622 = `𓆼𓆼𓆼𓆼𓍢𓍢𓍢𓍢𓍢𓍢𓎆𓎆𓏤𓏤` ·
[Egyptian numerals](https://en.wikipedia.org/wiki/Egyptian_numerals)

### Chinese numerals: `pn_hanzi`
Each digit is followed by a multiplier, 千 (1000), 百 (100) or 十 (10),
and 万 (10 000) splits large numbers in two. Skipped digits are marked
with a space, where modern text writes 零. 0 to 99 999 999.

4622 = `四千六百二十二`, 1005 = `一千␣五` ·
[Chinese numerals](https://en.wikipedia.org/wiki/Chinese_numerals)

### Counting rods: `pn_rods`
Rods laid on a counting board in columns: true place-value decimal.
Columns alternate between two forms of each digit so neighbours stay
distinct, and an empty column is left empty. 0 to 10^19 − 1.

23 = `𝍪␣𝍢`, 101 = `𝍠␣␣𝍠` ·
[Counting rods](https://en.wikipedia.org/wiki/Counting_rods)

### Brahmi numerals: `pn_brahmi`
The ancestors of our digits, but not yet positional: a separate sign
for each of 1 to 9 and 10 to 90, and a joiner 𑁿 that multiplies the
hundred and thousand signs. 0 to 9999.

256 = `𑁤𑁿𑁓𑁟𑁗` (hundred × two, fifty, six) ·
[Brahmi numerals](https://en.wikipedia.org/wiki/Brahmi_numerals)

### Maya numerals: `pn_mayan`
Base 20 in dots (ones) and bars (fives), with a real zero drawn as a
shell 𝋠, one sign per place. Renders and parses `pn_base20` values.

429 = `𝋡␣𝋡␣𝋩` (1 × 400 + 1 × 20 + 9) ·
[Maya numerals](https://en.wikipedia.org/wiki/Maya_numerals)

### Inca quipu: `pn_quipu`
Decimal numbers tied as knots on a cord. Unicode has no knot signs, so
• stands for a simple knot and ∞ for a turn of the long knot used only
in the ones place. An empty place has no knots. 0 to 10^19 − 1.

731 = `•••••••␣•••␣∞` ·
[Quipu](https://en.wikipedia.org/wiki/Quipu)

### Greek numerals: `pn_greek`
The alphabet as digits: 27 letters for 1 to 900, the lower keraia ͵ to
mark thousands, and the keraia ʹ to mark a number. 0 to 999 999.

2019 = `͵β␣ιθʹ` ·
[Greek numerals](https://en.wikipedia.org/wiki/Greek_numerals)

### Roman numerals: `pn_roman`
I V X L C D M, added up, with the six subtractive pairs IV IX XL XC CD
CM. The standard modern form only: no IIII. 0 to 3999.

1999 = `MCMXCIX` ·
[Roman numerals](https://en.wikipedia.org/wiki/Roman_numerals)

### Worked examples

Every entry below was written by the library itself. The base-60 and
base-20 columns use the modern notation; ␣ marks a space that is part
of the numeral.

| n | Base 60 | Base 20 | Brahmi | Greek | Chinese | Roman | Maya | Rods | Egyptian | Quipu | Cuneiform |
|--:|:--|:--|:--|:--|:--|:--|:--|:--|:--|:--|:--|
| 1 | 1 | 1 | 𑁒 | αʹ | 一 | I | 𝋡 | 𝍠 | 𓏤 | ∞ | 𒁹 |
| 2 | 2 | 2 | 𑁓 | βʹ | 二 | II | 𝋢 | 𝍡 | 𓏤𓏤 | ∞∞ | 𒁹𒁹 |
| 3 | 3 | 3 | 𑁔 | γʹ | 三 | III | 𝋣 | 𝍢 | 𓏤𓏤𓏤 | ∞∞∞ | 𒁹𒁹𒁹 |
| 4 | 4 | 4 | 𑁕 | δʹ | 四 | IV | 𝋤 | 𝍣 | 𓏤𓏤𓏤𓏤 | ∞∞∞∞ | 𒁹𒁹𒁹𒁹 |
| 5 | 5 | 5 | 𑁖 | εʹ | 五 | V | 𝋥 | 𝍤 | 𓏤𓏤𓏤𓏤𓏤 | ∞∞∞∞∞ | 𒁹𒁹𒁹𒁹𒁹 |
| 6 | 6 | 6 | 𑁗 | ϛʹ | 六 | VI | 𝋦 | 𝍥 | 𓏤𓏤𓏤𓏤𓏤𓏤 | ∞∞∞∞∞∞ | 𒁹𒁹𒁹𒁹𒁹𒁹 |
| 7 | 7 | 7 | 𑁘 | ζʹ | 七 | VII | 𝋧 | 𝍦 | 𓏤𓏤𓏤𓏤𓏤𓏤𓏤 | ∞∞∞∞∞∞∞ | 𒁹𒁹𒁹𒁹𒁹𒁹𒁹 |
| 8 | 8 | 8 | 𑁙 | ηʹ | 八 | VIII | 𝋨 | 𝍧 | 𓏤𓏤𓏤𓏤𓏤𓏤𓏤𓏤 | ∞∞∞∞∞∞∞∞ | 𒁹𒁹𒁹𒁹𒁹𒁹𒁹𒁹 |
| 9 | 9 | 9 | 𑁚 | θʹ | 九 | IX | 𝋩 | 𝍨 | 𓏤𓏤𓏤𓏤𓏤𓏤𓏤𓏤𓏤 | ∞∞∞∞∞∞∞∞∞ | 𒁹𒁹𒁹𒁹𒁹𒁹𒁹𒁹𒁹 |
| 10 | 10 | 10 | 𑁛 | ιʹ | 一十 | X | 𝋪 | 𝍩␣ | 𓎆 | •␣ | 𒌋 |
| 100 | 1,40 | 5,0 | 𑁤 | ρʹ | 一百 | C | 𝋥␣𝋠 | 𝍠␣␣ | 𓍢 | •␣␣ | 𒁹␣𒌋𒌋𒌋𒌋 |
| 1000 | 16,40 | 2,10,0 | 𑁥 | ͵αʹ | 一千 | M | 𝋢␣𝋪␣𝋠 | 𝍩␣␣␣ | 𓆼 | •␣␣␣ | 𒌋𒁹𒁹𒁹𒁹𒁹𒁹␣𒌋𒌋𒌋𒌋 |

## Mathematics

Early number theory on whole numbers up to 2^64 − 1.

### Euclid
- `pn_math_greatest_common_divisor` and `pn_math_least_common_multiple`,
  by Euclid's algorithm (*Elements*, Book VII): gcd(1071, 462) = 21
- `pn_math_factorize` splits a number into its prime factors:
  90 = 2 × 3 × 3 × 5
- `pn_math_is_perfect` finds numbers equal to the sum of their other
  divisors, like 6 = 1 + 2 + 3 (*Elements*, Book IX)

Further reading: [Euclid's Elements](https://en.wikipedia.org/wiki/Euclid%27s_Elements), [Euclidean algorithm](https://en.wikipedia.org/wiki/Euclidean_algorithm)

### Primes
- `pn_math_is_prime` and `pn_math_next_prime` are exact for every
  `uint64_t`: trial division below 2^32, and Miller-Rabin with Jim
  Sinclair's seven bases above
- `pn_math_sieve` lists the primes up to 1 000 000 with the sieve of
  Eratosthenes: 25 below 100, 78 498 below 1 000 000

Further reading: [Sieve of Eratosthenes](https://en.wikipedia.org/wiki/Sieve_of_Eratosthenes), [Miller-Rabin primality test](https://en.wikipedia.org/wiki/Miller%E2%80%93Rabin_primality_test)

### Pythagoras
- `pn_math_is_square` tests for square numbers, which the Pythagoreans
  drew as square arrays of dots
- `pn_math_pythagorean_triple` builds the sides of a right triangle
  with Euclid's formula (m² − n², 2mn, m² + n²), and
  `pn_math_is_pythagorean_triple` checks one: (119, 120, 169) is the
  first row of the Babylonian tablet Plimpton 322

Further reading: [Pythagorean theorem](https://en.wikipedia.org/wiki/Pythagorean_theorem), [Plimpton 322](https://en.wikipedia.org/wiki/Plimpton_322)

## Planned
- [Inca mathematics](https://en.wikipedia.org/wiki/Inca_mathematics) beyond quipu notation
- Archimedes
