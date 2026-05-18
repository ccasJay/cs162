#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
utils_dir="$repo_root/src/utils"
macros_file="$repo_root/src/misc/gdb-macros"

detect_project() {
  local cwd="$PWD"

  for project in threads userprog vm filesys; do
    if [[ "$cwd" == "$repo_root/src/$project/build"* ]]; then
      printf '%s\n' "$project"
      return 0
    fi
  done

  for project in threads userprog vm filesys; do
    if [[ "$cwd" == "$repo_root/src/$project"* ]]; then
      printf '%s\n' "$project"
      return 0
    fi
  done

  if [[ -f "$cwd/kernel.o" ]]; then
    local parent
    parent="$(basename "$(dirname "$cwd")")"
    case "$parent" in
      threads|userprog|vm|filesys)
        printf '%s\n' "$parent"
        return 0
        ;;
    esac
  fi

  printf '%s\n' "threads"
}

default_kernel_args() {
  local project="$1"
  case "$project" in
    threads)
      printf 'run\nalarm-multiple\n'
      ;;
    userprog|vm|filesys)
      printf 'run\necho x\n'
      ;;
    *)
      printf 'run\nhalt\n'
      ;;
  esac
}

cleanup_simulators() {
  pkill -x qemu-system-i386 >/dev/null 2>&1 || true
  pkill -x qemu-system-x86_64 >/dev/null 2>&1 || true
  pkill -x bochs >/dev/null 2>&1 || true
  pkill -x bochs-dbg >/dev/null 2>&1 || true
  pkill -f "$utils_dir/pintos --gdb" >/dev/null 2>&1 || true
  pkill -f "squish-pty bochs -q" >/dev/null 2>&1 || true
  sleep 0.2
}

project="$(detect_project)"
build_dir="$repo_root/src/$project/build"

if [[ ! -d "$build_dir" ]]; then
  echo "错误：找不到 build 目录：$build_dir"
  echo "请先在 src/$project 下完成编译。"
  exit 1
fi

if [[ ! -f "$build_dir/kernel.o" ]]; then
  echo "错误：找不到 $build_dir/kernel.o"
  echo "请先在 src/$project/build 下编译生成 kernel.o。"
  exit 1
fi

if [[ $# -gt 0 ]]; then
  kernel_args=("$@")
else
  mapfile -t kernel_args < <(default_kernel_args "$project")
fi

if ! command -v pintos-gdb >/dev/null 2>&1; then
  echo "错误：未找到 pintos-gdb，请确认 src/utils 已加入 PATH。"
  exit 1
fi

log_file="$build_dir/qemu.log"
cleanup_simulators
rm -f "$log_file"

echo "使用项目：$project"
echo "使用 build 目录：$build_dir"
echo "启动命令：pintos --gdb -- ${kernel_args[*]}"

(
  cd "$build_dir"
  PATH="$utils_dir:$PATH" pintos --gdb -- "${kernel_args[@]}"
) >"$log_file" 2>&1 &

sim_pid=$!
cleanup() {
  if kill -0 "$sim_pid" >/dev/null 2>&1; then
    kill "$sim_pid" >/dev/null 2>&1 || true
    wait "$sim_pid" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

for _ in {1..20}; do
  if ! kill -0 "$sim_pid" >/dev/null 2>&1; then
    echo "错误：Pintos 启动失败或已退出。"
    echo "请查看日志：$log_file"
    exit 1
  fi
  sleep 0.2
done

cd "$build_dir"
PATH="$utils_dir:$PATH" exec pintos-gdb -x "$macros_file" kernel.o -ex "debugpintos"
