#!/bin/bash
shopt -s extglob

mod=$1
build=$2

source_files=$(find "$mod" -name '*.c' -or -iname '*.s' | tr '\n' ' ')
dirs=$(find "$mod" -type d | tr '\n' ' ')

cat << EOF > $build/$mod.mk
SOURCE_FILES := $source_files
\$(BUILD_DIR)/$mod.ko: SOURCE_FILES := \$(SOURCE_FILES)
\$(BUILD_DIR)/$mod.ko: \$(OBJ_FILES)
\$(BUILD_DIR)/$mod.mk: $dirs
EOF