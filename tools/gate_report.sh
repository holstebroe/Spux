#!/bin/sh
# Acceptance gate (DESIGN_GUIDELINES 7): runs the gate's benches and prints the numbers.
#   tools/gate_report.sh <gate build dir> <Acidus checkout> <Tannhauser checkout>
# The build dir is configured with -DSPUX_GATE_ACIDUS_DIR and -DSPUX_GATE_TANNHAUSER_DIR (tests/gate) and
# built with `--target spux_gate` in Release. Instruction counts need valgrind (callgrind): instructions
# per frame are (count for 3N frames - count for N frames) / 2N, so start-up cancels out.
set -e
B="$1/tests/gate"
SPUX=$(cd "$(dirname "$0")/.." && pwd)

echo "== Lines of layout and widget code"
python3 "$SPUX/tools/gate_lines.py" "$2" "$3"

echo
echo "== Pixels (each step's frame hash must agree between the variants)"
for s in retro classic; do
  a=$("$B/acidus_gate_bench_retained_$s" --pixels | md5sum)
  b=$("$B/acidus_gate_bench_immediate_$s" --pixels | md5sum)
  c=$(SPUX_GATE_NO_CACHE=1 "$B/acidus_gate_bench_immediate_$s" --pixels | md5sum)
  echo "  Acidus $s: retained vs immediate $( [ "$a" = "$b" ] && echo identical || echo DIFFERENT ), cached vs uncached $( [ "$b" = "$c" ] && echo identical || echo DIFFERENT )"
done
"$B/tannhauser_gate_bench_retained_classic" --pixels > /tmp/spux_gate_t_r.txt
"$B/tannhauser_gate_bench_immediate_classic" --pixels > /tmp/spux_gate_t_i.txt
steps=$(wc -l < /tmp/spux_gate_t_r.txt)
same=$(paste -d'|' /tmp/spux_gate_t_r.txt /tmp/spux_gate_t_i.txt | awk -F'|' '$1 == $2' | wc -l)
echo "  Tannhauser classic: $same of $steps steps identical"

echo
echo "== CPU per frame, wall clock (median and mean of the best of 5 runs)"
for s in classic retro; do for v in retained immediate; do "$B/acidus_gate_bench_${v}_$s"; done; done
for v in retained immediate; do "$B/tannhauser_gate_bench_${v}_classic"; done

if command -v valgrind > /dev/null; then
  echo
  echo "== CPU per frame, instructions (callgrind)"
  ir() { # binary scenario frames -> instructions
    valgrind --tool=callgrind --callgrind-out-file=/tmp/spux_gate.cg "$1" "$2" "$3" > /dev/null 2>&1
    callgrind_annotate /tmp/spux_gate.cg 2> /dev/null | awk '/PROGRAM TOTALS/ { gsub(",", "", $1); print $1; exit }'
  }
  perframe() { # binary scenario N
    a=$(ir "$1" "$2" "$3"); b=$(ir "$1" "$2" $(($3 * 3)))
    echo $(( (b - a) / ($3 * 2) ))
  }
  for s in classic retro; do
    for sc in idle drag automation; do
      r=$(perframe "$B/acidus_gate_bench_retained_$s" --$sc 60); i=$(perframe "$B/acidus_gate_bench_immediate_$s" --$sc 60)
      echo "  Acidus $s $sc: retained $r, immediate $i ($(awk "BEGIN { printf \"%+.1f\", ($i - $r) * 100 / $r }") %)"
    done
  done
  for sc in idle hover drag automation; do
    r=$(perframe "$B/tannhauser_gate_bench_retained_classic" --$sc 40); i=$(perframe "$B/tannhauser_gate_bench_immediate_classic" --$sc 40)
    echo "  Tannhauser classic $sc: retained $r, immediate $i ($(awk "BEGIN { printf \"%+.1f\", ($i - $r) * 100 / $r }") %)"
  done
fi

echo
echo "== Binary size (whole plugin; stripped)"
for m in "$B"/*_gate_*.clap; do
  strip -o /tmp/spux_gate_stripped.so "$m"
  printf "  %-36s %8d bytes, stripped %8d\n" "$(basename "$m")" "$(stat -c %s "$m")" "$(stat -c %s /tmp/spux_gate_stripped.so)"
done
