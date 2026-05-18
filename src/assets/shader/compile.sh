#!/usr/bin/env bash
# compile.sh — UI 着色器编译脚本（Linux / macOS）
#
# 功能：
#   - 自动搜索 dxc 可执行文件
#   - 增量检查：源文件比输出文件新时才重新编译
#   - Linux 仅编译 SPIR-V（DXC on Linux 通常无 DXIL 支持）
#   - Windows/macOS 同时编译 SPIR-V + DXIL
#   - 任意编译失败立即退出
#
# 用法：
#   ./compile.sh              # 正常编译（增量）
#   ./compile.sh --force      # 强制全量重新编译
#   ./compile.sh --spv-only   # 只编译 SPIR-V（不管平台）
#   DXC=/path/to/dxc ./compile.sh  # 指定 dxc 路径

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ─── 参数解析 ────────────────────────────────────────────
FORCE=0
SPV_ONLY=0
for arg in "$@"; do
    case "$arg" in
        --force)    FORCE=1 ;;
        --spv-only) SPV_ONLY=1 ;;
        -h|--help)
            echo "Usage: $(basename "$0") [--force] [--spv-only]"
            echo "  --force     Force recompile all shaders regardless of timestamps"
            echo "  --spv-only  Only compile SPIR-V (skip DXIL)"
            exit 0
            ;;
        *) echo "Unknown argument: $arg"; exit 1 ;;
    esac
done

# ─── 平台检测 ─────────────────────────────────────────────
OS="$(uname -s 2>/dev/null || echo Windows)"
case "$OS" in
    Linux*)   PLATFORM=linux ;;
    Darwin*)  PLATFORM=macos ;;
    MINGW*|CYGWIN*|MSYS*) PLATFORM=windows ;;
    *)        PLATFORM=windows ;;
esac

# Linux 下 DXC 通常不支持 DXIL，自动设置 spv-only
if [[ "$PLATFORM" == "linux" && "$SPV_ONLY" -eq 0 ]]; then
    echo "[INFO] Linux platform detected: defaulting to SPIR-V only (DXIL requires Windows DXC)"
    SPV_ONLY=1
fi

# ─── 搜索 dxc ────────────────────────────────────────────
find_dxc() {
    # 1. 环境变量优先
    if [[ -n "${DXC:-}" && -x "$DXC" ]]; then
        echo "$DXC"; return 0
    fi

    # 2. PATH 中搜索
    if command -v dxc &>/dev/null; then
        command -v dxc; return 0
    fi

    # 3. 常见安装路径（Linux）
    local linux_paths=(
        /usr/bin/dxc
        /usr/local/bin/dxc
        "${VULKAN_SDK:-/nonexistent}/bin/dxc"
        /opt/vulkan-sdk/bin/dxc
    )
    for p in "${linux_paths[@]}"; do
        [[ -x "$p" ]] && { echo "$p"; return 0; }
    done

    # 4. 常见安装路径（Windows via MSYS2/Git Bash）
    local win_paths=(
        "D:/vulkan/Bin/dxc.exe"
        "D:/Vulkan/Bin/dxc.exe"
        "${VULKAN_SDK:-C:/VulkanSDK}/Bin/dxc.exe"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/dxc.exe"
    )
    for p in "${win_paths[@]}"; do
        [[ -x "$p" ]] && { echo "$p"; return 0; }
    done

    return 1
}

if ! DXC_BIN="$(find_dxc)"; then
    echo "[ERROR] dxc not found."
    echo "  Linux install:   sudo apt install directx-shader-compiler"
    echo "                   or install Vulkan SDK from https://vulkan.lunarg.com"
    echo "  Windows install: install Vulkan SDK, dxc.exe is in \$VULKAN_SDK/Bin"
    echo "  Manual path:     DXC=/path/to/dxc $0"
    exit 1
fi

echo "[INFO] Using dxc: $DXC_BIN"
"$DXC_BIN" --version 2>&1 | head -1 || true

# ─── 增量检查辅助函数 ────────────────────────────────────
# needs_rebuild <output> <dep1> [dep2 ...]
# 返回 0 表示需要（重新）编译，1 表示跳过
needs_rebuild() {
    local output="$1"; shift
    [[ "$FORCE" -eq 1 ]] && return 0
    [[ ! -f "$output" ]] && return 0
    for dep in "$@"; do
        [[ "$dep" -nt "$output" ]] && return 0
    done
    return 1
}

# ─── 编译任务 ─────────────────────────────────────────────
COMPILED=0
SKIPPED=0

compile_spv() {
    local stage="$1"    # vs / ps
    local profile="$2"  # vs_6_0 / ps_6_0
    local entry="$3"    # main_vs / main_ps
    local src="$4"      # vert.hlsl / frag.hlsl
    local out="$5"      # vert.spv / frag.spv

    if needs_rebuild "$out" "$src" common.hlsl; then
        echo "[COMPILE] $src -> $out (SPIR-V, $profile)"
        "$DXC_BIN" -spirv -T "$profile" -E "$entry" \
            -fspv-target-env=vulkan1.3 \
            -Fo "$out" "$src"
        COMPILED=$((COMPILED + 1))
    else
        echo "[SKIP]    $out is up-to-date"
        SKIPPED=$((SKIPPED + 1))
    fi
}

compile_dxil() {
    local profile="$1"  # vs_6_6 / ps_6_6
    local entry="$2"    # main_vs / main_ps
    local src="$3"      # vert.hlsl / frag.hlsl
    local out="$4"      # vert.dxil / frag.dxil

    if needs_rebuild "$out" "$src" common.hlsl; then
        echo "[COMPILE] $src -> $out (DXIL, $profile)"
        "$DXC_BIN" -T "$profile" -E "$entry" \
            -Fo "$out" "$src"
        COMPILED=$((COMPILED + 1))
    else
        echo "[SKIP]    $out is up-to-date"
        SKIPPED=$((SKIPPED + 1))
    fi
}

echo ""
echo "=== Compiling UI Shaders ==="

# Vertex shader — SPIR-V
compile_spv vs vs_6_0 main_vs vert.hlsl vert.spv

# Fragment shader — SPIR-V
compile_spv ps ps_6_0 main_ps frag.hlsl frag.spv

if [[ "$SPV_ONLY" -eq 0 ]]; then
    # Vertex shader — DXIL (Windows only)
    compile_dxil vs_6_0 main_vs vert.hlsl vert.dxil

    # Fragment shader — DXIL (Windows only)
    compile_dxil ps_6_0 main_ps frag.hlsl frag.dxil
fi

echo ""
echo "=== Done: $COMPILED compiled, $SKIPPED skipped ==="
if [[ "$SPV_ONLY" -eq 1 ]]; then
    echo "    (DXIL skipped — use --force --spv-only=0 on Windows to also generate DXIL)"
fi
