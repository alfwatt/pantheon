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
-       the pn_sex60 core rebased in base twenty: transcription, format, uint64 and double utils
- [x] [Maya numerals](https://en.wikipedia.org/wiki/Maya_numerals)
-       bar-and-dot places in UTF-8 from block U+1D2E0..U+1D2F3, the shell for zero drawn as a sign

#### Inca
- [ ] [Inca mathematics](https://en.wikipedia.org/wiki/Inca_mathematics 
- [x] [Quipu notation](https://en.wikipedia.org/wiki/Quipu)
-       positional decimal knots in UTF-8: simple knots (bullets) above the units line, the figure-eight and long-knot turns at it, zero the absence of a knot, 0..10^19-1

### Worked examples

Every rendering below came out of the library itself
(`pn_*_render`, `pn_*_format`), not off a chart; `U+2423 OPEN BOX`
shows a real trailing place-boundary space the cords, boards and
rows keep — it is part of the rendering.

```
    1  sex60 1    cuneiform 𒁹    vigesimal 1    mayan 𝋡    egyptian 𓏤    hanzi 一    rods 𝍠    brahmi 𑁒    quipu ∞
    2  sex60 2    cuneiform 𒁹𒁹    vigesimal 2    mayan 𝋢    egyptian 𓏤𓏤    hanzi 二    rods 𝍡    brahmi 𑁓    quipu ∞∞
    3  sex60 3    cuneiform 𒁹𒁹𒁹    vigesimal 3    mayan 𝋣    egyptian 𓏤𓏤𓏤    hanzi 三    rods 𝍢    brahmi 𑁔    quipu ∞∞∞
    4  sex60 4    cuneiform 𒁹𒁹𒁹𒁹    vigesimal 4    mayan 𝋤    egyptian 𓏤𓏤𓏤𓏤    hanzi 四    rods 𝍣    brahmi 𑁕    quipu ∞∞∞∞
    5  sex60 5    cuneiform 𒁹𒁹𒁹𒁹𒁹    vigesimal 5    mayan 𝋥    egyptian 𓏤𓏤𓏤𓏤𓏤    hanzi 五    rods 𝍤    brahmi 𑁖    quipu ∞∞∞∞∞
    6  sex60 6    cuneiform 𒁹𒁹𒁹𒁹𒁹𒁹    vigesimal 6    mayan 𝋦    egyptian 𓏤𓏤𓏤𓏤𓏤𓏤    hanzi 六    rods 𝍥    brahmi 𑁗    quipu ∞∞∞∞∞∞
    7  sex60 7    cuneiform 𒁹𒁹𒁹𒁹𒁹𒁹𒁹    vigesimal 7    mayan 𝋧    egyptian 𓏤𓏤𓏤𓏤𓏤𓏤𓏤    hanzi 七    rods 𝍦    brahmi 𑁘    quipu ∞∞∞∞∞∞∞
    8  sex60 8    cuneiform 𒁹𒁹𒁹𒁹𒁹𒁹𒁹𒁹    vigesimal 8    mayan 𝋨    egyptian 𓏤𓏤𓏤𓏤𓏤𓏤𓏤𓏤    hanzi 八    rods 𝍧    brahmi 𑁙    quipu ∞∞∞∞∞∞∞∞
    9  sex60 9    cuneiform 𒁹𒁹𒁹𒁹𒁹𒁹𒁹𒁹𒁹    vigesimal 9    mayan 𝋩    egyptian 𓏤𓏤𓏤𓏤𓏤𓏤𓏤𓏤𓏤    hanzi 九    rods 𝍨    brahmi 𑁚    quipu ∞∞∞∞∞∞∞∞∞
   # the structural values: blanks, shells, grouping
   10  sex60 10    cuneiform 𒌋    vigesimal 10    mayan 𝋪    egyptian 𓎆    hanzi 一十    rods 𝍩␣    brahmi 𑁛    quipu •␣
  100  sex60 1,40    cuneiform 𒁹␣𒌋𒌋𒌋𒌋    vigesimal 5,0    mayan 𝋥␣𝋠    egyptian 𓍢    hanzi 一百    rods 𝍠␣␣    brahmi 𑁤    quipu •␣␣
 1000  sex60 16,40    cuneiform 𒌋𒁹𒁹𒁹𒁹𒁹𒁹␣𒌋𒌋𒌋𒌋    vigesimal 2,10,0    mayan 𝋢␣𝋪␣𝋠    egyptian 𓆼    hanzi 一千    rods 𝍩␣␣␣    brahmi 𑁥    quipu •␣␣␣
```
### Mathematics

#### Greek
- Pythagoras
    - [pythagorean theorem](https://en.wikipedia.org/wiki/Pythagorean_theorem)
- Euclid
    - [Elements](https://en.wikipedia.org/wiki/Euclid%27s_Elements)
        - Geometry
- Archimedes
