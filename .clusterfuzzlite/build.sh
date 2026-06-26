#!/bin/bash -eu

: "${SRC:=$(pwd)}"
: "${OUT:=$SRC/out}"
: "${CXX:=clang++}"
: "${CXXFLAGS:=}"
: "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"

mkdir -p "$OUT"
BUILD_DIR="$OUT/private-fuzzing-project-obj"
mkdir -p "$BUILD_DIR"

COMMON_FLAGS=(-std=c++17 -I"$SRC/include" -I"$SRC")
SOURCES=(
  src/format.cpp
  src/decoder.cpp
  src/parser.cpp
  src/deserializer.cpp
  src/validator.cpp
)

OBJECTS=()
for source in "${SOURCES[@]}"; do
  base="$(basename "${source%.cpp}")"
  object="$BUILD_DIR/$base.o"
  $CXX $CXXFLAGS "${COMMON_FLAGS[@]}" -c "$SRC/$source" -o "$object"
  OBJECTS+=("$object")
done

$CXX $CXXFLAGS "${COMMON_FLAGS[@]}" "$SRC/fuzz/parser_fuzzer.cc" \
  "${OBJECTS[@]}" $LIB_FUZZING_ENGINE -o "$OUT/parser_fuzzer"

$CXX $CXXFLAGS "${COMMON_FLAGS[@]}" "$SRC/fuzz/decoder_fuzzer.cc" \
  "${OBJECTS[@]}" $LIB_FUZZING_ENGINE -o "$OUT/decoder_fuzzer"

$CXX $CXXFLAGS "${COMMON_FLAGS[@]}" "$SRC/fuzz/deserializer_fuzzer.cc" \
  "${OBJECTS[@]}" $LIB_FUZZING_ENGINE -o "$OUT/deserializer_fuzzer"
