# libPantheon

On beyond `math.h`

## Goal

Encapsulate functions from the [history of mathematics](https://en.wikipedia.org/wiki/History_of_mathematics)
and science in well documented, easy to understand implementations
which are efficient enough for use in highly demanding applications:
no heap allocation, small by-value types, straight-line code.

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
- chinese numerals

#### Indian
- vedic numerals

### Mathematics

#### Greek

- Pythagoras
    - [pythagorean theorem](https://en.wikipedia.org/wiki/Pythagorean_theorem)

- Euclid
    - [Elements](https://en.wikipedia.org/wiki/Euclid%27s_Elements)
        - Geometry

- Archimedes
