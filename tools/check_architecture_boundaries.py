#!/usr/bin/env python3
"""Architecture boundary guard for VMP-ui.

This is a soft P0 guard: existing boundary debt is explicitly baselined, while
new occurrences fail the check. It also prints the Phase 0 architecture metrics
that are meaningful before the corresponding debt reaches zero. The baseline
should only shrink over time.
"""

from __future__ import annotations

import argparse
from collections import Counter
from pathlib import Path
import re
import sys


API_INCLUDE_RE = re.compile(r'#\s*include\s+"api/[^"\n]+"')
STATIC_RUNTIME_RE = re.compile(
    r"\b(?:"
    r"Registry::(?:TryGet|Get|View|AnyOf|AllOf|Clear|Emplace|Remove|Valid)|"
    r"Dispatcher::(?:Trigger|Enqueue|Sink|Update)"
    r")\b"
)
COMMON_RUNTIME_INCLUDE_RE = re.compile(r'#\s*include\s+"(?:core/RuntimeFacade\.hpp|singleton/Registry\.hpp)"')
PUBLIC_API_FORBIDDEN_INCLUDE_RE = re.compile(
    r'#\s*include\s+[<"](?:(?:src/)?helper/[^">]+|(?:src/)?detail/[^">]+|(?:src/)?entt(?:/[^">]+)?|(?:src/)?common/components/[^">]+|(?:src/)?common/(?:Animation|ErrorCodes|Result|Policies)\.hpp|(?:src/)?traits/[^">]+|core/RuntimeFacade\.hpp|singleton/Registry\.hpp|singleton/Dispatcher\.hpp)'
)
ENTT_ENTITY_RE = re.compile(r"\bentt::entity\b")
ENTT_NAMESPACE_RE = re.compile(r"\bentt::")
UI_RUNTIME_CURRENT_RE = re.compile(r"UiRuntime::current\(\)")
RAW_ACCESS_RE = re.compile(r"\b(?:Registry|Dispatcher)::raw\(\)|\.raw\(\)")
UI_SOURCES_BLOCK_RE = re.compile(r"set\(UI_SOURCES\s+(.*?)\)", re.DOTALL)
DETAIL_CPP_ENTRY_RE = re.compile(r"\b(detail/[A-Za-z0-9_]+\.cpp)\b")
PUBLIC_INCLUDE_BLOCK_RE = re.compile(r"target_include_directories\(ui\s+(.*?)\)", re.DOTALL)
PUBLIC_LINK_BLOCK_RE = re.compile(r"target_link_libraries\(ui\s+PUBLIC\s+(.*?)\)", re.DOTALL)
QUEUED_EVENT_DISPATCH_RE = re.compile(r"\bDispatchQueued\s*\(")

INTERNAL_API_INCLUDE_DIRS = (
    Path("src/core"),
    Path("src/detail"),
    Path("src/systems"),
    Path("src/renderers"),
    Path("src/services"),
)
STATIC_RUNTIME_DIRS = (Path("src/systems"),)
COMMON_DIRS = (Path("src/common"),)
PUBLIC_API_HEADER_DIRS = (Path("src/api"), Path("include"))
API_CPP_DIRS = (Path("src/api"),)
RUNTIME_ACCESS_DIRS = (Path("src"),)
RAW_ACCESS_DIRS = (Path("src"),)
SOURCE_EXTENSIONS = {".h", ".hh", ".hpp", ".hxx", ".c", ".cc", ".cpp", ".cxx"}
HEADER_EXTENSIONS = {".h", ".hh", ".hpp", ".hxx"}
IMPLEMENTATION_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx"}

# Existing debt as of 2026-06-04. Keep exact occurrence counts so new usages fail
# even when a file already has one allowed include/call of the same kind.
ALLOWED_API_INCLUDE_COUNTS = Counter(
    {
    }
)

ALLOWED_STATIC_RUNTIME_COUNTS = Counter({})

ALLOWED_COMMON_RUNTIME_INCLUDE_COUNTS = Counter(
    {}
)

ALLOWED_API_CPP_RUNTIME_CURRENT_COUNTS = Counter({})

ALLOWED_API_CPP_ENTT_ENTITY_COUNTS = Counter(
    {
        # 2026-08-16: P2-1 ListView (CreateListView/AddListItem) 新增 8 处后移除死代码 ApplyListItemVisualState（21 -> 29 -> 28）
        ("src/api/Factory.cpp", "entt::entity"): 27,
        ("src/api/Utils.cpp", "entt::entity"): 6,
        # 2026-10-03: Qt Quick anchors 公共 API 实现（SetAnchors 参数校验 + 依赖环检测）
        # 在公共边界转换/校验内部 entity；待迁移到 src/systems/AnchorSolver 后移除此条目。
        ("src/api/Layout.cpp", "entt::entity"): 8,
    }
)

ALLOWED_RUNTIME_CURRENT_COUNTS = Counter()

ALLOWED_PUBLIC_CMAKE_DEBT_COUNTS = Counter({})

ALLOWED_RAW_ACCESS_COUNTS = Counter({})

ALLOWED_UNLISTED_DETAIL_CPP_COUNTS = Counter({})


class Violation:
    def __init__(self, rule: str, path: str, line_no: int, text: str) -> None:
        self.rule = rule
        self.path = path
        self.line_no = line_no
        self.text = text.strip()

    def __str__(self) -> str:
        return f"[{self.rule}] {self.path}:{self.line_no}: {self.text}"


def normalized_relative(path: Path, root: Path) -> str:
    return path.relative_to(root).as_posix()


def is_under(path: Path, root: Path, prefixes: tuple[Path, ...]) -> bool:
    rel = path.relative_to(root)
    return any(rel == prefix or prefix in rel.parents for prefix in prefixes)


def iter_source_files(root: Path):
    for path in root.rglob("*"):
        if not path.is_file() or path.suffix not in SOURCE_EXTENSIONS:
            continue
        rel = path.relative_to(root).as_posix()
        if rel.startswith("third_party/") or rel.startswith("build/"):
            continue
        yield path


def public_section_tokens(block: str) -> list[str]:
    """Return tokens after PUBLIC and before the next visibility section."""

    tokens = re.findall(r"[^\s()]+", block)
    if "PUBLIC" not in tokens:
        return []
    start = tokens.index("PUBLIC") + 1
    result: list[str] = []
    for token in tokens[start:]:
        if token in {"PRIVATE", "INTERFACE"}:
            break
        result.append(token)
    return result


def count_matches(root: Path):
    api_counts: Counter[tuple[str, str]] = Counter()
    runtime_counts: Counter[tuple[str, str]] = Counter()
    common_counts: Counter[tuple[str, str]] = Counter()
    public_api_forbidden_include_counts: Counter[tuple[str, str]] = Counter()
    public_api_entt_counts: Counter[tuple[str, str]] = Counter()
    api_cpp_runtime_counts: Counter[tuple[str, str]] = Counter()
    api_cpp_entt_entity_counts: Counter[tuple[str, str]] = Counter()
    runtime_current_counts: Counter[tuple[str, str]] = Counter()
    raw_access_counts: Counter[tuple[str, str]] = Counter()
    unlisted_detail_cpp_counts: Counter[tuple[str, str]] = Counter()
    public_cmake_debt_counts: Counter[tuple[str, str]] = Counter()
    queued_event_dispatch_sites: list[tuple[str, int, str]] = []
    locations: dict[tuple[str, str, str], list[tuple[int, str]]] = {}

    cmake_file = root / "src" / "CMakeLists.txt"
    ui_sources_entries: set[str] = set()
    if cmake_file.exists():
        try:
            cmake_text = cmake_file.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            cmake_text = cmake_file.read_text(encoding="utf-8-sig")
        if match := UI_SOURCES_BLOCK_RE.search(cmake_text):
            ui_sources_entries = set(DETAIL_CPP_ENTRY_RE.findall(match.group(1)))

        if match := PUBLIC_INCLUDE_BLOCK_RE.search(cmake_text):
            for token in public_section_tokens(match.group(1)):
                if token not in {"${CMAKE_SOURCE_DIR}", "${CMAKE_CURRENT_SOURCE_DIR}"}:
                    continue
                debt = f"PUBLIC include: {token}"
                key = ("src/CMakeLists.txt", debt)
                public_cmake_debt_counts[key] += 1
                locations.setdefault(("public-cmake-debt", *key), []).append((1, debt))

        if match := PUBLIC_LINK_BLOCK_RE.search(cmake_text):
            for token in re.findall(r"[^\s()]+", match.group(1)):
                if token not in {"EnTT::EnTT", "Eigen3::Eigen", "spdlog::spdlog", "spdlog::spdlog_header_only"}:
                    continue
                debt = f"PUBLIC link: {token}"
                key = ("src/CMakeLists.txt", debt)
                public_cmake_debt_counts[key] += 1
                locations.setdefault(("public-cmake-debt", *key), []).append((1, debt))

    for path in root.glob("src/detail/*.cpp"):
        rel = normalized_relative(path, root)
        cmake_entry = rel.removeprefix("src/")
        if cmake_entry not in ui_sources_entries:
            key = (rel, "detail-cpp-not-in-ui-sources")
            unlisted_detail_cpp_counts[key] += 1
            locations.setdefault(("detail-cpp-not-in-ui-sources", *key), []).append((1, cmake_entry))

    for path in root.glob("src/detail/*"):
        if path.is_file():
            rel = path.relative_to(root).as_posix()
            key = (rel, "detail-file-forbidden")
            unlisted_detail_cpp_counts[key] += 1
            locations.setdefault(("detail-cpp-not-in-ui-sources", *key), []).append((1, rel))

    migrated_api_headers = {
        "Animation.hpp",
        "Canvas.hpp",
        "Chains.hpp",
        "Controls.hpp",
        "Entity.hpp",
        "Event.hpp",
        "Factory.hpp",
        "Hierarchy.hpp",
        "Icon.hpp",
        "Image.hpp",
        "Table.hpp",
        "Layout.hpp",
        "Log.hpp",
        "Query.hpp",
        "Scale.hpp",
        "Size.hpp",
        "State.hpp",
        "Theme.hpp",
        "Text.hpp",
        "Timer.hpp",
        "Utils.hpp",
        "Visibility.hpp",
    }
    for header_name in migrated_api_headers:
        legacy_path = root / "src" / "api" / header_name
        if legacy_path.exists():
            rel = legacy_path.relative_to(root).as_posix()
            key = (rel, "legacy-api-header-forbidden")
            unlisted_detail_cpp_counts[key] += 1
            locations.setdefault(("detail-cpp-not-in-ui-sources", *key), []).append((1, rel))

    animation_compatibility_header = root / "src" / "common" / "Animation.hpp"
    expected_animation_forwarder = [
        '#pragma once',
        '#include "ui/TweenOptions.hpp" // IWYU pragma: export',
    ]

    def normalize_whitespace(line: str) -> str:
        """折叠连续空白为单空格，使比较对 clang-format 注释对齐产生的空格差异不敏感。"""
        return re.sub(r"\s+", " ", line.strip())

    animation_forwarder_lines = [
        normalize_whitespace(line)
        for line in animation_compatibility_header.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]
    if animation_forwarder_lines != expected_animation_forwarder:
        rel = normalized_relative(animation_compatibility_header, root)
        key = (rel, "animation-compatibility-header-not-pure-forwarder")
        unlisted_detail_cpp_counts[key] += 1
        locations.setdefault(("detail-cpp-not-in-ui-sources", *key), []).append(
            (1, "expected only #pragma once and ui/TweenOptions.hpp include")
        )

    for path in iter_source_files(root):
        rel = normalized_relative(path, root)
        try:
            lines = path.read_text(encoding="utf-8").splitlines()
        except UnicodeDecodeError:
            lines = path.read_text(encoding="utf-8-sig").splitlines()

        for line_no, line in enumerate(lines, start=1):
            if is_under(path, root, INTERNAL_API_INCLUDE_DIRS):
                if match := API_INCLUDE_RE.search(line):
                    key = (rel, match.group(0))
                    api_counts[key] += 1
                    locations.setdefault(("api-include", *key), []).append((line_no, line))

            if is_under(path, root, STATIC_RUNTIME_DIRS):
                for match in STATIC_RUNTIME_RE.finditer(line):
                    key = (rel, match.group(0))
                    runtime_counts[key] += 1
                    locations.setdefault(("static-runtime", *key), []).append((line_no, line))

            if is_under(path, root, COMMON_DIRS):
                if match := COMMON_RUNTIME_INCLUDE_RE.search(line):
                    key = (rel, match.group(0))
                    common_counts[key] += 1
                    locations.setdefault(("common-runtime-include", *key), []).append((line_no, line))

            if path.suffix in HEADER_EXTENSIONS and is_under(path, root, PUBLIC_API_HEADER_DIRS):
                if match := PUBLIC_API_FORBIDDEN_INCLUDE_RE.search(line):
                    key = (rel, match.group(0))
                    public_api_forbidden_include_counts[key] += 1
                    locations.setdefault(("public-api-forbidden-include", *key), []).append((line_no, line))
                for match in ENTT_NAMESPACE_RE.finditer(line):
                    key = (rel, match.group(0))
                    public_api_entt_counts[key] += 1
                    locations.setdefault(("public-api-entt", *key), []).append((line_no, line))

            if path.suffix in IMPLEMENTATION_EXTENSIONS and is_under(path, root, API_CPP_DIRS):
                for match in ENTT_ENTITY_RE.finditer(line):
                    key = (rel, match.group(0))
                    api_cpp_entt_entity_counts[key] += 1
                    locations.setdefault(("api-cpp-entt-entity", *key), []).append((line_no, line))

            if is_under(path, root, RUNTIME_ACCESS_DIRS):
                # UiRuntime.hpp 是 UiRuntime::current() 的定义处，其文档/断言字符串
                # 含 "UiRuntime::current()" 字样会导致误判，此处显式排除。
                # src/api 是无 Runtime 公共 API 到 Application 内部调用上下文的
                # 唯一适配边界；门禁只禁止 Runtime 反查重新扩散到其他生产模块。
                if rel != "src/core/UiRuntime.hpp" and not rel.startswith("src/api/"):
                    for match in UI_RUNTIME_CURRENT_RE.finditer(line):
                        key = (rel, match.group(0))
                        runtime_current_counts[key] += 1
                        locations.setdefault(("runtime-current", *key), []).append((line_no, line))

            if (
                rel.startswith("src/")
                and (path.suffix in IMPLEMENTATION_EXTENSIONS or rel == "src/core/TaskChain.hpp")
                and rel not in {"src/api/Event.cpp"}
                and QUEUED_EVENT_DISPATCH_RE.search(line)
            ):
                queued_event_dispatch_sites.append((rel, line_no, line.strip()))

            if is_under(path, root, RAW_ACCESS_DIRS):
                for match in RAW_ACCESS_RE.finditer(line):
                    key = (rel, match.group(0))
                    raw_access_counts[key] += 1
                    locations.setdefault(("raw-access", *key), []).append((line_no, line))

    return (
        api_counts,
        runtime_counts,
        common_counts,
        public_api_forbidden_include_counts,
        public_api_entt_counts,
        api_cpp_runtime_counts,
        api_cpp_entt_entity_counts,
        runtime_current_counts,
        raw_access_counts,
        unlisted_detail_cpp_counts,
        public_cmake_debt_counts,
        queued_event_dispatch_sites,
        locations,
    )


def collect_extra(
    rule: str,
    actual: Counter[tuple[str, str]],
    allowed: Counter[tuple[str, str]],
    locations: dict[tuple[str, str, str], list[tuple[int, str]]],
) -> list[Violation]:
    violations: list[Violation] = []
    for key, count in sorted(actual.items()):
        extra = count - allowed.get(key, 0)
        if extra <= 0:
            continue

        path, token = key
        all_locations = locations.get((rule, path, token), [])
        for line_no, line in all_locations[-extra:]:
            violations.append(Violation(rule, path, line_no, line))

    return violations


def collect_stale_baseline(
    rule: str,
    actual: Counter[tuple[str, str]],
    allowed: Counter[tuple[str, str]],
) -> list[Violation]:
    """Fail when a baseline entry is larger than current debt.

    This turns the guard from "no new debt" into "baseline only shrinks". When
    a debt occurrence disappears, the matching allowed counter must be removed
    in the same change so old debt cannot silently remain baselined forever.
    """

    violations: list[Violation] = []
    for key, count in sorted(allowed.items()):
        current = actual.get(key, 0)
        if current >= count:
            continue

        path, token = key
        text = f"stale baseline allows {count}, current debt is {current}: {token}"
        violations.append(Violation(f"{rule}-stale-baseline", path, 0, text))

    return violations


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Check VMP-ui architecture boundary debt does not grow.")
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="Repository root")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    (
        api_counts,
        runtime_counts,
        common_counts,
        public_api_forbidden_include_counts,
        public_api_entt_counts,
        api_cpp_runtime_counts,
        api_cpp_entt_entity_counts,
        runtime_current_counts,
        raw_access_counts,
        unlisted_detail_cpp_counts,
        public_cmake_debt_counts,
        queued_event_dispatch_sites,
        locations,
    ) = count_matches(root)

    violations: list[Violation] = []
    violations.extend(collect_extra("api-include", api_counts, ALLOWED_API_INCLUDE_COUNTS, locations))
    violations.extend(collect_stale_baseline("api-include", api_counts, ALLOWED_API_INCLUDE_COUNTS))
    violations.extend(collect_extra("static-runtime", runtime_counts, ALLOWED_STATIC_RUNTIME_COUNTS, locations))
    violations.extend(collect_stale_baseline("static-runtime", runtime_counts, ALLOWED_STATIC_RUNTIME_COUNTS))
    violations.extend(
        collect_extra("common-runtime-include", common_counts, ALLOWED_COMMON_RUNTIME_INCLUDE_COUNTS, locations)
    )
    violations.extend(
        collect_stale_baseline("common-runtime-include", common_counts, ALLOWED_COMMON_RUNTIME_INCLUDE_COUNTS)
    )
    violations.extend(
        collect_extra("public-api-forbidden-include", public_api_forbidden_include_counts, Counter(), locations)
    )
    violations.extend(collect_extra("public-api-entt", public_api_entt_counts, Counter(), locations))
    violations.extend(
        collect_extra(
            "api-cpp-runtime-current", api_cpp_runtime_counts, ALLOWED_API_CPP_RUNTIME_CURRENT_COUNTS, locations
        )
    )
    violations.extend(
        collect_stale_baseline("api-cpp-runtime-current", api_cpp_runtime_counts, ALLOWED_API_CPP_RUNTIME_CURRENT_COUNTS)
    )
    violations.extend(
        collect_extra("api-cpp-entt-entity", api_cpp_entt_entity_counts, ALLOWED_API_CPP_ENTT_ENTITY_COUNTS, locations)
    )
    violations.extend(
        collect_stale_baseline("api-cpp-entt-entity", api_cpp_entt_entity_counts, ALLOWED_API_CPP_ENTT_ENTITY_COUNTS)
    )
    violations.extend(collect_extra("runtime-current", runtime_current_counts, ALLOWED_RUNTIME_CURRENT_COUNTS, locations))
    violations.extend(collect_stale_baseline("runtime-current", runtime_current_counts, ALLOWED_RUNTIME_CURRENT_COUNTS))
    violations.extend(
        collect_extra(
            "public-cmake-debt", public_cmake_debt_counts, ALLOWED_PUBLIC_CMAKE_DEBT_COUNTS, locations
        )
    )
    violations.extend(
        collect_stale_baseline(
            "public-cmake-debt", public_cmake_debt_counts, ALLOWED_PUBLIC_CMAKE_DEBT_COUNTS
        )
    )
    violations.extend(collect_extra("raw-access", raw_access_counts, ALLOWED_RAW_ACCESS_COUNTS, locations))
    violations.extend(collect_stale_baseline("raw-access", raw_access_counts, ALLOWED_RAW_ACCESS_COUNTS))
    violations.extend(
        collect_extra(
            "detail-cpp-not-in-ui-sources",
            unlisted_detail_cpp_counts,
            ALLOWED_UNLISTED_DETAIL_CPP_COUNTS,
            locations,
        )
    )
    violations.extend(
        collect_stale_baseline(
            "detail-cpp-not-in-ui-sources", unlisted_detail_cpp_counts, ALLOWED_UNLISTED_DETAIL_CPP_COUNTS
        )
    )
    if len(queued_event_dispatch_sites) != 1:
        violations.append(
            Violation(
                rule="queued-event-frame-dispatch-site-count",
                path="src/core/TaskChain.hpp",
                line_no=1,
                text=f"expected exactly 1 production dispatch site, found {len(queued_event_dispatch_sites)}",
            )
        )

    print("Architecture metrics:")
    print(f"  UiRuntime::current() calls: {sum(runtime_current_counts.values())}")
    print(f"  PUBLIC internal include paths: {sum(1 for _, token in public_cmake_debt_counts if token.startswith('PUBLIC include:'))}")
    print(f"  PUBLIC internal dependency links: {sum(1 for _, token in public_cmake_debt_counts if token.startswith('PUBLIC link:'))}")
    print(f"  queued event frame dispatch sites: {len(queued_event_dispatch_sites)}")
    for path, line_no, text in queued_event_dispatch_sites:
        print(f"    {path}:{line_no}: {text}")

    if violations:
        print("Architecture boundary check failed: new boundary debt detected.", file=sys.stderr)
        print("Existing debt is baselined; remove entries from the baseline when debt is fixed.", file=sys.stderr)
        for violation in violations:
            print(str(violation), file=sys.stderr)
        return 1

    print("Architecture boundary check passed: no new P0 boundary debt.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
