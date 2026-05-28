<#
.SYNOPSIS
    批量对项目自有源码运行 clang-format 与 clang-tidy，并生成 tidy 汇总报告。

.DESCRIPTION
    - 文件清单来自 `git ls-files`，过滤掉 third_party / extern / build 等目录。
    - clang-format 默认原地写入；-Check 时改为 --dry-run --Werror。
    - clang-tidy 依赖 compile_commands.json（默认 build/），按文件并行调用。
    - tidy 结果聚合为 Markdown 报告，按 check 名计数并列出每文件诊断。

.PARAMETER Mode
    format | tidy | all (默认 all)

.PARAMETER Check
    仅检查不落盘。format 走 --dry-run --Werror；tidy 仍写报告但不修改源文件。

.PARAMETER BuildDir
    compile_commands.json 所在目录，默认 build

.PARAMETER ReportPath
    tidy 汇总报告输出路径，默认 docs/clang-tidy-report.md

.PARAMETER Jobs
    tidy 并发数，默认 = 逻辑 CPU 数
#>
[CmdletBinding()]
param(
    [ValidateSet('format', 'tidy', 'all')]
    [string]$Mode = 'all',
    [switch]$Check,
    [string]$BuildDir = 'build',
    [string]$ReportPath = 'docs/clang-tidy-report.md',
    [int]$Jobs = [Environment]::ProcessorCount
)

$ErrorActionPreference = 'Stop'

# 定位仓库根
$repoRoot = (git rev-parse --show-toplevel) 2>$null
if (-not $repoRoot) { throw '当前目录不是 git 仓库。' }
Set-Location $repoRoot

function Resolve-Tool([string]$name) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if (-not $cmd) { throw "找不到 $name，请先安装或加入 PATH。" }
    return $cmd.Source
}

# 收集自有源码
$extensions = @('.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx')
$excludePattern = '^(third_party|extern|external|vendor|build|out|cmake-build[^/]*|\.git)/'

$allFiles = git ls-files |
    Where-Object { $_ -notmatch $excludePattern } |
    Where-Object { $extensions -contains ([System.IO.Path]::GetExtension($_).ToLowerInvariant()) }

if (-not $allFiles) { Write-Warning '未匹配到任何自有源码。'; return }

$headerExt = @('.h', '.hh', '.hpp', '.hxx')
$sourceFiles = $allFiles | Where-Object {
    $headerExt -notcontains ([System.IO.Path]::GetExtension($_).ToLowerInvariant())
}

Write-Host "[lint-and-format] 命中文件: $($allFiles.Count) (其中 TU: $($sourceFiles.Count))" -ForegroundColor Cyan

$exitCode = 0

# clang-format
if ($Mode -in 'format', 'all') {
    $clangFormat = Resolve-Tool 'clang-format'
    Write-Host "[clang-format] 使用 $clangFormat" -ForegroundColor Cyan
    if ($Check) {
        & $clangFormat --style=file --dry-run --Werror @allFiles
        if ($LASTEXITCODE -ne 0) { $exitCode = $LASTEXITCODE; Write-Warning 'clang-format --dry-run 失败。' }
    } else {
        & $clangFormat --style=file -i @allFiles
        if ($LASTEXITCODE -ne 0) { $exitCode = $LASTEXITCODE; Write-Warning 'clang-format -i 失败。' }
    }
}

# clang-tidy
if ($Mode -in 'tidy', 'all') {
    $clangTidy = Resolve-Tool 'clang-tidy'
    $ccPath = Join-Path $repoRoot (Join-Path $BuildDir 'compile_commands.json')
    if (-not (Test-Path $ccPath)) {
        throw "找不到 $ccPath，请先运行 CMake 配置 (生成 compile_commands.json)。"
    }
    Write-Host "[clang-tidy] 使用 $clangTidy, 并发 $Jobs" -ForegroundColor Cyan

    $logDir = Join-Path $env:TEMP "vmp-tidy-$([guid]::NewGuid().ToString('N'))"
    New-Item -ItemType Directory -Path $logDir | Out-Null

    try {
        $sourceFiles | ForEach-Object -Parallel {
            $file = $_
            $safe = ($file -replace '[\\/:]', '_')
            $log = Join-Path $using:logDir "$safe.log"
            & $using:clangTidy --quiet -p $using:BuildDir $file 2>&1 | Set-Content -Path $log -Encoding UTF8
        } -ThrottleLimit $Jobs

        # 解析诊断
        $diagPattern = '^(?<file>[^:]+):(?<line>\d+):(?<col>\d+):\s+(?<level>warning|error):\s+(?<msg>.+?)\s+\[(?<check>[^\]]+)\]\s*$'
        $findings = @()
        Get-ChildItem $logDir -File | ForEach-Object {
            foreach ($line in Get-Content $_.FullName) {
                if ($line -match $diagPattern) {
                    $findings += [pscustomobject]@{
                        File  = ($Matches.file -replace [regex]::Escape($repoRoot + [IO.Path]::DirectorySeparatorChar), '') -replace '\\', '/'
                        Line  = [int]$Matches.line
                        Col   = [int]$Matches.col
                        Level = $Matches.level
                        Check = $Matches.check
                        Msg   = $Matches.msg
                    }
                }
            }
        }

        # 写报告
        $reportAbs = Join-Path $repoRoot $ReportPath
        New-Item -ItemType Directory -Path (Split-Path $reportAbs -Parent) -Force | Out-Null
        $sb = [System.Text.StringBuilder]::new()
        [void]$sb.AppendLine('# clang-tidy 汇总报告')
        [void]$sb.AppendLine('')
        [void]$sb.AppendLine("- 生成时间: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
        [void]$sb.AppendLine("- 扫描 TU 数: $($sourceFiles.Count)")
        [void]$sb.AppendLine("- 诊断条目: $($findings.Count)")
        [void]$sb.AppendLine("- compile_commands: $BuildDir/compile_commands.json")
        [void]$sb.AppendLine('')
        [void]$sb.AppendLine('## Summary (by check)')
        [void]$sb.AppendLine('')
        [void]$sb.AppendLine('| Check | Error | Warning | Total |')
        [void]$sb.AppendLine('|---|---:|---:|---:|')
        $findings | Group-Object Check | Sort-Object Count -Descending | ForEach-Object {
            $err = ($_.Group | Where-Object Level -EQ 'error').Count
            $warn = ($_.Group | Where-Object Level -EQ 'warning').Count
            [void]$sb.AppendLine("| ``$($_.Name)`` | $err | $warn | $($_.Count) |")
        }
        [void]$sb.AppendLine('')
        [void]$sb.AppendLine('## Findings by file')
        [void]$sb.AppendLine('')
        $findings | Group-Object File | Sort-Object Name | ForEach-Object {
            [void]$sb.AppendLine("### $($_.Name)")
            [void]$sb.AppendLine('')
            [void]$sb.AppendLine('| Line:Col | Level | Check | Message |')
            [void]$sb.AppendLine('|---|---|---|---|')
            $_.Group | Sort-Object Line, Col | ForEach-Object {
                $msg = $_.Msg -replace '\|', '\|'
                [void]$sb.AppendLine("| $($_.Line):$($_.Col) | $($_.Level) | ``$($_.Check)`` | $msg |")
            }
            [void]$sb.AppendLine('')
        }
        Set-Content -Path $reportAbs -Value $sb.ToString() -Encoding UTF8
        Write-Host "[clang-tidy] 报告已写入 $ReportPath" -ForegroundColor Green

        $errorCount = ($findings | Where-Object Level -EQ 'error').Count
        if ($errorCount -gt 0) {
            Write-Warning "clang-tidy 发现 $errorCount 个 error 级别诊断。"
            if ($Check) { $exitCode = 1 }
        }
    } finally {
        Remove-Item $logDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

exit $exitCode
