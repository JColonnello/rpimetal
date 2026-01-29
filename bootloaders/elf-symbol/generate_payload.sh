#!/bin/sh
set -e

prefix="$2"
S="$1/$prefix.S"
C="$1/$prefix.c"
shift 2

: > "$S"
# optional: ensure .section directive for rodata
printf ".section .rodata\n" >> "$S"
i=1
for f in "$@"; do
    printf '.global %s_f%d_start, %s_f%d_end\n'  "$prefix" "$i" "$prefix" "$i" >> "$S"
    printf '%s_f%d_start:\n' "$prefix" "$i" >> "$S"
    printf '.incbin "%s"\n' "$f" >> "$S"
    printf '%s_f%d_end:\n' "$prefix" "$i" >> "$S"
    i=$((i+1))
done

cat > "$C" <<'EOF'
/* GENERATED - do not edit */
#include <payload.h>
#include <attrib.h>
EOF

i=1
for f in "$@"; do
    printf 'extern char %s_f%d_start[], %s_f%d_end[];\n' "$prefix" "$i" "$prefix" "$i" >> "$C"
    i=$((i+1))
done

echo "struct payload_entry ${prefix}[] = {" >> "$C"
i=1
for f in "$@"; do
    printf '  { "%s", 0, %s_f%d_start },\n' "$f" "$prefix" "$i" >> "$C"
    i=$((i+1))
done

echo '};' >> "$C"
echo "const size_t ${prefix}_count = sizeof(${prefix})/sizeof(${prefix}[0]);" >> "$C"

# Generate initializer to compute sizes (must be called before use)
echo 'constructor static void payload_init(void) {' >> "$C"
i=1
for f in "$@"; do
    printf '  %s[%d].size = (size_t)(%s_f%d_end - %s_f%d_start);\n' "$prefix" "$((i-1))" "$prefix" "$i" "$prefix" "$i" >> "$C"
    i=$((i+1))
done
echo '}' >> "$C"
