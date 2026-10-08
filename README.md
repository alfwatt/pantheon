# libPantheon

The road to `math.h`

## Goal
Encapsulate functions from the [history of mathematics](https://en.wikipedia.org/wiki/History_of_mathematics) and science 
in well documented, easy to understand implementations which are 
efficient enough for use in highly demanding applications: 
- no heap allocation
- small by-value types
- straight-line code.

## Using it
```
make          # builds build/libpantheon.a and the test binaries
make test     # runs every test
```

Consume the library with `-Iinclude build/libpantheon.a` and a single
`#include "pantheon.h"`.

## Contents
### Pre-mathematics number systems

#### Babylonian
- [x] [sexagesimal](https://en.wikipedia.org/wiki/Sexagesimal) conversion
- parse/format scholarly transcriptions, convert to and from `uint64_t` and `double`
- [x] [babylonian numerals](https://en.wikipedia.org/wiki/Babylonian_numerals)
- positional cuneiform rendering in UTF-8

#### Egyptian
- [x] [egyptian numerals](https://en.wikipedia.org/wiki/Egyptian_numerals)
- hieroglyphic rendering in UTF-8: the seven power-of-ten signs, 0..9,999,999 with the scribe's blank for zero

#### Chinese
- [x] [chinese numerals](https://en.wikipedia.org/wiki/Chinese_numerals)
- formal hanzi reading in UTF-8: digits with their 千/百/十/万 multipliers, 0..99,999,999, one blank over each skipped digit
- [x] [Counting rods](https://en.wikipedia.org/wiki/Counting_rods)
- positional rods in UTF-8: the two alternating Unicode digit sets, 0..10^19−1 with the blank column for zero

#### Indian
- [x] [brahmi numerals](https://en.wikipedia.org/wiki/Brahmi_numerals)
-      non-positional decimal rendering in UTF-8: a dedicated sign for every unit 1..9 and ten 10..90, hundreds and thousand

#### Mayan
- [x] vigesimal conversion
-       the pn_base60 core rebased in base twenty: transcription, format, uint64 and double utils
- [x] [Maya numerals](https://en.wikipedia.org/wiki/Maya_numerals)
-       bar-and-dot places in UTF-8 from block U+1D2E0..U+1D2F3, the shell for zero drawn as a sign

#### Inca
- [ ] [Inca mathematics](https://en.wikipedia.org/wiki/Inca_mathematics 
- [x] [Quipu notation](https://en.wikipedia.org/wiki/Quipu)
-       positional decimal knots in UTF-8: simple knots (bullets) above the units line, the figure-eight and long-knot turns at it, zero the absence of a knot, 0..10^19-1

#### Greek
- [x] [Greek numerals](https://en.wikipedia.org/wiki/Greek_numerals)
-       Milesian alphabetic cords in UTF-8: 27 letter-signs over three descending rows, the low keraia announcing the thousands, the trailing keraia proof of a number, one space splitting the runs, 0..999,999

#### Roman
- [x] [Roman numerals](https://en.wikipedia.org/wiki/Roman_numerals)
-       canonical tallied form in ASCII: I V X L C D M with the subtractive digraphs IV IX XL XC CD CM, no clock-maker IIII, Bede's nulla for zero, 0..3,999

### Worked examples

Every rendering below came out of the library itself
(`pn_*_render`, `pn_*_format`), not off a chart; the U+2423 OPEN
BOX shows a real place-boundary space the cords, boards and rows
keep — it is part of the rendering.  The two conversion cores
(`pn_base60`, `pn_vigesimal`) sit beside the base-ten value; the
rendered notations follow narrow-to-wide, longest furthest right.

```
   n     sex60   vigesimal   brahmi   greek   hanzi   roman   mayan   rods    egyptian    quipu       cuneiform
      -------------------------------------------------------------------------------------------------------------
    1     1       1           𑁒        αʹ      一      I       𝋡       𝍠      𓏤           ∞           𒁹
    2     2       2           𑁓        βʹ      二      II      𝋢       𝍡      𓏤𓏤          ∞∞          𒁹𒁹
    3     3       3           𑁔        γʹ      三      III     𝋣       𝍢      𓏤𓏤𓏤         ∞∞∞         𒁹𒁹𒁹
    4     4       4           𑁕        δʹ      四      IV      𝋤       𝍣      𓏤𓏤𓏤𓏤        ∞∞∞∞        𒁹𒁹𒁹𒁹
    5     5       5           𑁖        εʹ      五      V       𝋥       𝍤      𓏤𓏤𓏤𓏤𓏤       ∞∞∞∞∞       𒁹𒁹𒁹𒁹𒁹
    6     6       6           𑁗        ϛʹ      六      VI      𝋦       𝍥      𓏤𓏤𓏤𓏤𓏤𓏤      ∞∞∞∞∞∞      𒁹𒁹𒁹𒁹𒁹𒁹
    7     7       7           𑁘        ζʹ      七      VII     𝋧       𝍦      𓏤𓏤𓏤𓏤𓏤𓏤𓏤     ∞∞∞∞∞∞∞     𒁹𒁹𒁹𒁹𒁹𒁹𒁹
    8     8       8           𑁙        ηʹ      八      VIII    𝋨       𝍧      𓏤𓏤𓏤𓏤𓏤𓏤𓏤𓏤    ∞∞∞∞∞∞∞∞    𒁹𒁹𒁹𒁹𒁹𒁹𒁹𒁹
    9     9       9           𑁚        θʹ      九      IX      𝋩       𝍨      𓏤𓏤𓏤𓏤𓏤𓏤𓏤𓏤𓏤   ∞∞∞∞∞∞∞∞∞   𒁹𒁹𒁹𒁹𒁹𒁹𒁹𒁹𒁹

       the structural values: blanks, shells, grouping
           (n stays base ten; the rest wear their own bases)

   10     10      10          𑁛        ιʹ      一十    X       𝋪       𝍩␣     𓎆           •␣          𒌋
  100     1,40    5,0         𑁤        ρʹ      一百    C       𝋥␣𝋠     𝍠␣␣    𓍢           •␣␣         𒁹␣𒌋𒌋𒌋𒌋
 1000     16,40   2,10,0      𑁥        ͵αʹ     一千    M       𝋢␣𝋪␣𝋠   𝍩␣␣␣   𓆼           •␣␣␣        𒌋𒁹𒁹𒁹𒁹𒁹𒁹␣𒌋𒌋𒌋𒌋
```

### Mathematics

#### Greek
- Pythagoras
     - [x] [pythagorean theorem](https://en.wikipedia.org/wiki/Pythagorean_theorem)
- the Plimpton loom: `pn_math_pythagorean_triple` weaves legs m*m−n*n and 2*m*n and hypotenuse m*m+n*n from m > n ≥ 1; `pn_math_pythagorean_check` weighs the rod, sides to 4,294,967,295
     - [x] gnomon figures
- `pn_math_is_square` weighs any ledger number, squares to 18446744073709551615
- Euclid
     - [x] [Elements](https://en.wikipedia.org/wiki/Euclid%27s_Elements)
- the ladder of remainders VII.1-2: `pn_math_gcd` and `pn_math_lcm` over the whole uint64 ledger; the abacist's ledger of factors `pn_math_factorize` (90 = 2 x 3 x 3 x 5, ascending braid); the numbers equal to their own parts IX.36: `pn_math_perfect_check` (6, 28, 496, 8128)
         - Arithmetic
- `pn_math_is_prime` and `pn_math_next_prime`: proved on the whole ledger — Euclid's trial division below 2^32, Miller's seven certified witnesses above
- [x] [Sieve of Eratosthenes](https://en.wikipedia.org/wiki/Sieve_of_Eratosthenes)
- `pn_math_sieve` lays the papyrus in UTF-8-free uint64 rows to 1,000,000: pi(100) = 25, pi(1000000) = 78498
- Archimedes
