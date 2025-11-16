#!/usr/bin/env bash
# run_gvn_and_stats.sh
# 1) Run opt N times to produce time-trace JSONs
# 2) Extract "Total GVNPass" .dur (µs) and compute mean/stddev
#
# Usage:
#   ./run_gvn_and_stats.sh \
#     --opt build-timing/bin/opt \
#     --input input.ll \
#     --passes gvn \
#     --iter 10 \
#     --out traces_out \
#     [--discard-first] \
#     [--keep]              # keep also writes a merged CSV

set -euo pipefail

# ---- args ----
OPT_BIN=""
INPUT=""
PASSES="gvn"
ITER=10
OUTDIR="traces_out"
DISCARD_FIRST=false
KEEP=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --opt) OPT_BIN="$2"; shift 2;;
    --input) INPUT="$2"; shift 2;;
    --passes) PASSES="$2"; shift 2;;
    --iter) ITER="$2"; shift 2;;
    --out) OUTDIR="$2"; shift 2;;
    --discard-first) DISCARD_FIRST=true; shift;;
    --keep) KEEP=true; shift;;
    -h|--help)
      echo "Usage: $0 --opt <path/to/opt> --input <input.ll> [--passes gvn] [--iter 10] [--out traces_out] [--discard-first] [--keep]"
      exit 0;;
    *) echo "Unknown arg: $1" >&2; exit 1;;
  esac
done

if [[ -z "$OPT_BIN" || -z "$INPUT" ]]; then
  echo "Error: --opt and --input are required." >&2
  exit 1
fi

if ! command -v jq >/dev/null 2>&1; then
  echo "Error: jq is required but not found. Please install jq." >&2
  exit 1
fi

mkdir -p "$OUTDIR"

echo "Running $ITER iterations of: $OPT_BIN -passes=\"$PASSES\" $INPUT"
for ((i=1;i<=ITER;i++)); do
  trace="$OUTDIR/trace_${i}.json"
  echo "  Iter $i -> $trace"
  "$OPT_BIN" -passes="$PASSES" "$INPUT" -o /dev/null \
    -time-trace -time-trace-file="$trace" >/dev/null 2>&1
done

# ---- extract GVN durations (µs) ----
vals=()
files=()
for f in "$OUTDIR"/trace_*.json; do
  [[ -e "$f" ]] || continue

  # Use jq to extract durations line by line (works on macOS too)
  durs=$(jq -r '.traceEvents[] | select(.name=="Total GVNPass") | .dur' "$f" 2>/dev/null || true)
  if [ -z "$durs" ]; then
    echo "Warning: '$f' has no 'Total GVNPass' event; skipping." >&2
    continue
  fi

  sum=0
  while IFS= read -r d; do
    if [[ "$d" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
      sum=$(awk -v a="$sum" -v b="$d" 'BEGIN{printf("%.6f", a+b)}')
    fi
  done <<< "$durs"

  vals+=("$sum")
  files+=("$f")
done

total=${#vals[@]}
if (( total == 0 )); then
  echo "Error: no usable 'Total GVNPass' timings found." >&2
  exit 1
fi

# 可選：丟掉第一筆（冷啟動）
start_index=0
if $DISCARD_FIRST; then
  start_index=1
  if (( total <= 1 )); then
    echo "Error: cannot discard-first with only $total sample(s)." >&2
    exit 1
  fi
fi

# 印出每次
echo "---------------------------------------------"
echo "Per-run GVN dur (µs):"
for ((i=start_index;i<total;i++)); do
  run=$((i+1))
  printf "Run %2d: %-8.3f   (%s)\n" "$run" "${vals[$i]}" "${files[$i]}"
done

# 計算平均/標準差（樣本標準差）
read -r mean stddev count <<< "$(
  for ((i=start_index;i<total;i++)); do echo "${vals[$i]}"; done | awk '
  BEGIN{count=0; mean=0.0; M2=0.0;}
  {
    x=$1+0.0;
    count++;
    delta = x - mean;
    mean += delta / count;
    delta2 = x - mean;
    M2 += delta * delta2;
  }
  END{
    if (count > 1) {
      variance = M2 / (count - 1);
      stddev = sqrt(variance);
    } else {
      stddev = 0.0;
    }
    printf("%.6f %.6f %d\n", mean, stddev, count);
  }'
)"

echo "---------------------------------------------"
echo "Samples: $count (from total $total; discard-first: $DISCARD_FIRST)"
printf "Mean  (µs): %.3f\n" "$mean"
printf "StdDev(µs): %.3f\n" "$stddev"

# 需要保留 CSV 的話
if $KEEP; then
  csv="$OUTDIR/gvn_timings.csv"
  {
    echo "run,file,gvn_us"
    for ((i=0;i<total;i++)); do
      run=$((i+1))
      printf "%d,%s,%.6f\n" "$run" "${files[$i]}" "${vals[$i]}"
    done
  } > "$csv"
  echo "CSV saved: $csv"
fi
