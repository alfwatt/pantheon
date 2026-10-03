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
      (parse/format scholarly transcriptions, convert to and from
      `uint64_t` and `double`)
- [x] [babylonian numerals](https://en.wikipedia.org/wiki/Babylonian_numerals)
      (positional cuneiform rendering in UTF-8)

#### Egyptian
- [x] [egyptian numerals](https://en.wikipedia.org/wiki/Egyptian_numerals)
       (hieroglyphic rendering in UTF-8: the seven power-of-ten signs,
       0..9,999,999 with the scribe's blank for zero)

#### Chinese
- [x] [chinese numerals](https://en.wikipedia.org/wiki/Chinese_numerals)
        (formal hanzi reading in UTF-8: digits with their 千/百/十/万
        multipliers, 0..99,999,999, one blank over each skipped digit)
- [x] [counting rods](https://en.wikipedia.org/wiki/Counting_rods)
        (positional rods in UTF-8: the two alternating Unicode digit
        sets, 0..10^19−1 with the blank column for zero)

#### Indian
- [x] [brahmi numerals](https://en.wikipedia.org/wiki/Brahmi_numerals)
         (non-positional decimal rendering in UTF-8: a dedicated sign
        for every unit 1..9 and ten 10..90, hundreds and thousands
        ligatured with U+1107F, 0..9,999 with the blank for zero)
- vedic numerals

#### Mayan
- Vigesimal conversion
- Mayan numerals
### Mathematics

#### Greek

- Pythagoras
    - [pythagorean theorem](https://en.wikipedia.org/wiki/Pythagorean_theorem)

- Euclid
    - [Elements](https://en.wikipedia.org/wiki/Euclid%27s_Elements)
        - Geometry

- Archimedes
