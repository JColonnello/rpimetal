#!/bin/bash
shopt -s extglob

mod=$1
build=$2

source_files=$(find "$mod" -name '*.c' -or -iname '*.s' | tr '\n' ' ')
dirs=$(find "$mod" -type d | tr '\n' ' ')

cat << EOF > $build/$mod.mk
SOURCE_FILES := $source_files
OBJ_FILES := \$(SOURCE_FILES:%=\$(BUILD_DIR)/%.o)
DIRS := $dirs
MODULE_DIR := $mod

\$(BUILD_DIR)/\$(MODULE_DIR).ko: \$(OBJ_FILES)
	\$(CC) -r \$(filter-out %.mk,\$^) -o \$@

\$(BUILD_DIR)/\$(MODULE_DIR).mk: \$(DIRS)

-include \$(OBJ_FILES:%.o=%.d)
EOF