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
    printf ".global payload_f%d_start, payload_f%d_end\n" "$i" "$i" >> "$S"
    printf "payload_f%d_start:\n" "$i" >> "$S"
    printf '.incbin "%s"\n' "$f" >> "$S"
    printf "payload_f%d_end:\n" "$i" >> "$S"
    i=$((i+1))
done

cat > "$C" <<'EOF'
/* GENERATED - do not edit */
#include <payload.h>
#include <attrib.h>
EOF

i=1
for f in "$@"; do
    printf 'extern char payload_f%d_start[], payload_f%d_end[];\n' "$i" "$i" >> "$C"
    i=$((i+1))
done

echo "struct payload_entry ${prefix}[] = {" >> "$C"
i=1
for f in "$@"; do
    printf '  { "%s", 0, payload_f%d_start },\n' "$f" "$i" >> "$C"
    i=$((i+1))
done

echo '};' >> "$C"
echo "const size_t ${prefix}_count = sizeof(${prefix})/sizeof(${prefix}[0]);" >> "$C"

# Generate initializer to compute sizes (must be called before use)
echo 'constructor static void payload_init(void) {' >> "$C"
i=1
for f in "$@"; do
    printf "  ${prefix}[%d].size = (size_t)(payload_f%d_end - payload_f%d_start);\n" "$((i-1))" "$i" "$i" >> "$C"
    i=$((i+1))
done
echo '}' >> "$C"
