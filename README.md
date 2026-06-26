# Framed Data Format Toolkit

Framed Data Format Toolkit is a deterministic C++ parser and object model for the Framed Data Format (FDF), a compact structured binary file format used for exercising parser, decoder, deserializer, and validator code paths under fuzzing.

## File Format

An FDF file begins with a 16-byte header:

- magic header: `FDF2`
- major and minor version bytes
- file flags
- section count
- header size
- rolling checksum over the section body

Each section has a fixed descriptor with a four-byte tag, encoding type, flags, record count, decoded length, encoded payload length, and payload checksum. Supported section tags are:

- `STRT`: length-prefixed string table
- `META`: typed metadata records
- `DATA`: named data blocks, including optional inline encoded blocks
- `CMND`: nested command records with typed arguments
- `EVNT`: event records with attributes and length-prefixed payloads
- `XBLK`: extended data-block section

The decoder supports raw payloads, XOR-RLE streams, token-table packing, and delta streams. Checksums are validation signals rather than the only way to enter the deeper parser, which keeps fuzzing focused on structured parser behavior.

## Normal Build

```sh
cmake -S . -B build
cmake --build build
```

Run the CLI on an FDF file:

```sh
./build/fdf_inspect fuzz/corpus/parser_fuzzer/valid_full.fdf
```

On Windows with a multi-config generator, the executable may be under `build/Debug/`.

## Tests

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The tests construct valid binary documents in memory and verify parsing, decoding, nested command reconstruction, validation, and deserialization.

## Fuzzing

ClusterFuzzLite uses `.clusterfuzzlite/build.sh` as the entry point. The script compiles every harness and writes the final executables into `$OUT`.

Local libFuzzer-style build:

```sh
mkdir -p out/fuzz
SRC="$PWD" OUT="$PWD/out/fuzz" CXX=clang++ \
  CXXFLAGS="-g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer" \
  LIB_FUZZING_ENGINE="-fsanitize=fuzzer" \
  .clusterfuzzlite/build.sh
```

Harnesses:

- `parser_fuzzer`: calls the real binary parser and parsed-file validator.
- `decoder_fuzzer`: drives every block decoder mode directly.
- `deserializer_fuzzer`: parses bytes, reconstructs a document object, and validates the deserialized document.

Seed corpus layout:

```text
fuzz/corpus/parser_fuzzer/
fuzz/corpus/decoder_fuzzer/
fuzz/corpus/deserializer_fuzzer/
```

The dictionary in `fuzz/dictionary.txt` contains magic bytes, section tags, common string-table entries, command words, delimiters, and useful little-endian numeric fragments.
