#!/usr/bin/env bash
# Spike benchmark: the v1 engine path (plain mpv window, d3d11 + gpu-next, same scalers as v1's mpv.conf)
# against the v2 spike (libmpv render API inside Qt Quick), with and without glass panels.
# Each run: 1280x720 window (logical; DPI-scaled the same way in both), tracks 1+2 mixed, muted, N seconds.
# Usage: [RUNS=3] [VARIANTS="v1 v2-glass"] [TOGGLE=1] bash tools/bench.sh <seconds> <video> [video...]
#   RUNS    runs per variant (default 3); the table shows the median first frame and every run's drops
#   VARIANTS  space-separated subset of: v1 v2 v2-glass v2-second (default: v1 v2 v2-glass)
#             v2-second = opening a second video while v2 already runs: the first instance plays, a second
#             launch hands the file over and exits; "startup" is then the hand-over -> playback restart time
#   TOGGLE  also run v1-toggle / v2-toggle: mix <-> track 1 alone every 2 s while playing
# Columns: vo drops = running total over the whole run (frame-drop-count resets on every mixer switch);
#          first frame = mpv log clock (starts at mpv init; v2's starts after Qt init);
#          startup = v2 only, OS process creation -> first playback restart (the honest end-to-end number);
#          vo inits = "VO:" lines (1 = video never re-initialized, e.g. by an audio switch).
# Needs: tools/setup-dev.sh done, v2 built (bash tools/check.sh v2). Logs: .out/bench/
set -euo pipefail
cd "$(dirname "$0")/.."
. .out/devenv.sh
seconds="${1:?usage: bash tools/bench.sh <seconds> <video> [video...]}"; shift
[ $# -gt 0 ] || { echo "usage: bash tools/bench.sh <seconds> <video> [video...]"; exit 3; }
RUNS="${RUNS:-3}"; TOGGLE="${TOGGLE:-0}"
SPIKE=.out/build-v2/echoplay.exe
[ -x "$SPIKE" ] || { echo "v2 not built: run bash tools/check.sh v2"; exit 3; }
rm -rf .out/bench && mkdir -p .out/bench
MIX='[aid1][aid2]amix=inputs=2:normalize=0[ao]'

v1() { # $1 = log, rest = extra mpv args
  local log="$1"; shift
  "$MPV_EXE" --no-config --vo=gpu-next --gpu-api=d3d11 --hwdec=auto-safe \
    --scale=lanczos --cscale=lanczos --dscale=mitchell --geometry="$V1_GEOMETRY" --mute=yes \
    --lavfi-complex="$MIX" --script=tools/bench/v1-probe.lua --msg-level=all=v --log-file="$log" "$@" >/dev/null 2>&1 || true
}
v2() { # $1 = log, rest = extra spike args
  local log="$1"; shift
  "$SPIKE" --bench "$seconds" --mix --log-file "$log" "$@" >/dev/null 2>&1 || true
}

v2second() { # $1 = log, $2 = video: a running instance, then a second launch that must exit by itself
  local log="$1" video="$2"
  "$SPIKE" --log-file "$log" "$video" >/dev/null 2>&1 &
  sleep 4
  timeout 15 "$SPIKE" "$video" >/dev/null 2>&1 || true
  sleep 3
  taskkill //F //IM echoplay.exe >/dev/null 2>&1 || true
  wait || true
}

variants="${VARIANTS:-v1 v2 v2-glass}"; [ "$TOGGLE" = 1 ] && variants="$variants v1-toggle v2-toggle"
i=0
for video in "$@"; do
  i=$((i + 1))
  for r in $(seq 1 "$RUNS"); do
    for v in $variants; do
      log=".out/bench/$i-$v-r$r.log"
      case $v in
        v1)        v1 "$log" --script-opts=bench-seconds="$seconds" "$video" ;;
        v1-toggle) v1 "$log" --script-opts=bench-seconds="$seconds",mix-toggle=yes "$video" ;;
        v2)        v2 "$log" --no-glass "$video" ;;
        v2-glass)  v2 "$log" "$video" ;;
        v2-toggle) v2 "$log" --mix-toggle "$video" ;;
        v2-second) v2second "$log" "$video" ;;
      esac
    done
  done
done

PYTHONIOENCODING=utf-8 python - "$RUNS" "$variants" "$@" <<'EOF'
import json, re, statistics, sys
runs, variants, videos = int(sys.argv[1]), sys.argv[2].split(), sys.argv[3:]
print(f"{'video':<30} {'run':<9} {'first frame':>11} {'startup':>8} {'vo drops':>10} {'dec drops':>10} {'delayed':>10} {'vo inits':>8}  hwdec / vo")
for i, video in enumerate(videos, 1):
    name = video.replace('\\', '/').split('/')[-1][:29]
    for v in variants:
        ff, st, drops, dec, dly, inits, hw = [], [], [], [], [], [], '?'
        for r in range(1, runs + 1):
            try:
                log = open(f'.out/bench/{i}-{v}-r{r}.log', encoding='utf-8', errors='replace').read()
            except OSError:
                continue
            m = re.search(r'^\[\s*([\d.]+)\].*first video frame after restart shown', log, re.M)
            if m: ff.append(float(m.group(1)))
            if v == 'v2-second':
                m = re.search(r'SECOND_OPEN (\{.*\})', log)
                if m: st.append(json.loads(m.group(1))['ms'] / 1000)
            else:
                m = re.search(r'STARTUP (\{.*\})', log)
                if m: st.append(json.loads(m.group(1))['process_ms'] / 1000)
            inits.append(len(re.findall(r'\]\[i\]\[cplayer\] VO: \[', log)))
            m = re.search(r'BENCH (\{.*\})', log)
            if not m:
                drops.append('x'); dec.append('x'); dly.append('x'); continue
            b = json.loads(m.group(1))
            drops.append(b.get('frame-drop-total', b.get('frame-drop-count')) or 0); dec.append(b.get('decoder-frame-drop-count') or 0)
            dly.append(b.get('vo-delayed-frame-count') or 0); hw = f"{b.get('hwdec-current')} / {b.get('current-vo')}"
        med = lambda xs: f"{statistics.median(xs):.2f} s" if xs else '-'
        j = lambda xs: '/'.join(map(str, xs))
        print(f"{name:<30} {v:<9} {med(ff):>11} {med(st):>8} {j(drops):>10} {j(dec):>10} {j(dly):>10} {j(inits):>8}  {hw}")
EOF
