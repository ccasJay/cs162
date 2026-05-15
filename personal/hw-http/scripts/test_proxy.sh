#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_DIR="$(mktemp -d)"
UPSTREAM_DIR="$TMP_DIR/upstream"
UPSTREAM_PORT="19090"
PROXY_PORT="18080"
UPSTREAM_PID=""
PROXY_PID=""

cleanup() {
  if [[ -n "$PROXY_PID" ]]; then
    kill "$PROXY_PID" >/dev/null 2>&1 || true
    wait "$PROXY_PID" 2>/dev/null || true
  fi
  if [[ -n "$UPSTREAM_PID" ]]; then
    kill "$UPSTREAM_PID" >/dev/null 2>&1 || true
    wait "$UPSTREAM_PID" 2>/dev/null || true
  fi
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT

mkdir -p "$UPSTREAM_DIR/subdir"
cat > "$UPSTREAM_DIR/index.html" <<'EOF'
<html><body><h1>proxy test home</h1></body></html>
EOF
cat > "$UPSTREAM_DIR/subdir/hello.txt" <<'EOF'
hello through proxy
EOF
python3 - <<'PY' > "$UPSTREAM_DIR/large.txt"
print('A' * 200000)
PY

python3 -m http.server "$UPSTREAM_PORT" --directory "$UPSTREAM_DIR" >/dev/null 2>&1 &
UPSTREAM_PID=$!

"$ROOT_DIR/httpserver" --proxy 127.0.0.1:"$UPSTREAM_PORT" --port "$PROXY_PORT" >/dev/null 2>&1 &
PROXY_PID=$!

wait_for_http() {
  local url="$1"
  for _ in $(seq 1 50); do
    if curl -sS --noproxy "*" "$url" >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.1
  done
  echo "Timed out waiting for $url" >&2
  return 1
}

assert_eq() {
  local expected="$1"
  local actual="$2"
  local message="$3"
  if [[ "$expected" != "$actual" ]]; then
    echo "FAIL: $message" >&2
    echo "Expected: $expected" >&2
    echo "Actual:   $actual" >&2
    exit 1
  fi
}

assert_file_eq() {
  local expected_file="$1"
  local actual_file="$2"
  local message="$3"
  if ! cmp -s "$expected_file" "$actual_file"; then
    echo "FAIL: $message" >&2
    diff -u "$expected_file" "$actual_file" || true
    exit 1
  fi
}

wait_for_http "http://127.0.0.1:$UPSTREAM_PORT/"
wait_for_http "http://127.0.0.1:$PROXY_PORT/"

echo "[1/4] Compare root response body"
curl -sS --noproxy "*" "http://127.0.0.1:$UPSTREAM_PORT/" > "$TMP_DIR/upstream_root.out"
curl -sS --noproxy "*" "http://127.0.0.1:$PROXY_PORT/" > "$TMP_DIR/proxy_root.out"
assert_file_eq "$TMP_DIR/upstream_root.out" "$TMP_DIR/proxy_root.out" "root body differs"

echo "[2/4] Compare nested file response body"
curl -sS --noproxy "*" "http://127.0.0.1:$UPSTREAM_PORT/subdir/hello.txt" > "$TMP_DIR/upstream_nested.out"
curl -sS --noproxy "*" "http://127.0.0.1:$PROXY_PORT/subdir/hello.txt" > "$TMP_DIR/proxy_nested.out"
assert_file_eq "$TMP_DIR/upstream_nested.out" "$TMP_DIR/proxy_nested.out" "nested file body differs"

echo "[3/4] Compare 404 status code"
upstream_status=$(curl -sS -o /dev/null -w '%{http_code}' --noproxy "*" "http://127.0.0.1:$UPSTREAM_PORT/missing")
proxy_status=$(curl -sS -o /dev/null -w '%{http_code}' --noproxy "*" "http://127.0.0.1:$PROXY_PORT/missing")
assert_eq "$upstream_status" "$proxy_status" "404 status code differs"

echo "[4/4] Compare large response body"
curl -sS --noproxy "*" "http://127.0.0.1:$UPSTREAM_PORT/large.txt" > "$TMP_DIR/upstream_large.out"
curl -sS --noproxy "*" "http://127.0.0.1:$PROXY_PORT/large.txt" > "$TMP_DIR/proxy_large.out"
assert_file_eq "$TMP_DIR/upstream_large.out" "$TMP_DIR/proxy_large.out" "large body differs"

echo "PASS: proxy smoke tests succeeded"
