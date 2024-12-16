#!/bin/bash -eu

shopt -s nullglob

make -C src/fuzz

cp src/fuzz/zforth_fuzzer src/fuzz/*.dict "$OUT"/
