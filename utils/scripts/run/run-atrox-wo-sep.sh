#!/usr/bin/env bash

LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib"

FILENAME=${1}
OUTPUT_FILENAME=${FILENAME%.*}-atrox.ll

opt \
  -load @Pedigree_LOCATION@ \
  -load @IteratorRecognition_LOCATION@ \
  -load ${LIB_DIR}/@PASS_SO_NAME@ \
  -S \
  -basicaa \
  -globals-aa \
  -scev-aa \
  -tbaa \
  -atrox-lbc-pass \
  -atrox-selection-strategy=witr \
  -debug-only=atrox-loop-body-clone \
  -debug-only=atrox-selector \
  -o ${OUTPUT_FILENAME} \
  ${FILENAME}

