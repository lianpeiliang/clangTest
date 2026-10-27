param(
    [string]$QtRoot = 'D:/Qt/Qt5.12.8',
    [string]$ClangRoot = 'D:/msys64/mingw64'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = $PSScriptRoot
$qtBin = Join-Path $QtRoot '5.12.8/mingw73_64/bin'
$compilerBin = Join-Path $QtRoot 'Tools/mingw730_64/bin'
$clangBin = Join-Path $ClangRoot 'bin'
$buildDir = Join-Path $projectRoot 'build/package-release'
$stageRoot = Join-Path $projectRoot ('build/package-staging/' + [guid]::NewGuid().ToString('N'))
$appDir = Join-Path $stageRoot 'clangTest-windows-x64'
$distDir = Join-Path $projectRoot 'dist'
$zipPath = Join-Path $distDir 'clangTest-windows-x64.zip'
$objdump = Join-Path $clangBin 'objdump.exe'

function Invoke-Checked {
    param([string]$File, [string[]]$Arguments)
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed (exit ${LASTEXITCODE}): $File" }
}

# 每次使用新的暂存目录，不删除已有构建产物。
$savedPath = $env:PATH
try {
    $env:PATH = "$compilerBin;$clangBin;$qtBin;" + $savedPath
    New-Item -ItemType Directory -Path $buildDir, $appDir, $distDir -Force | Out-Null
    Push-Location -LiteralPath $buildDir
    try {
        Write-Host 'Building Release...'
        Invoke-Checked (Join-Path $qtBin 'qmake.exe') @(
            (Join-Path $projectRoot 'clangTest.pro'), '-spec', 'win32-g++',
            "LIBCLANG_ROOT=$($ClangRoot.Replace('\', '/'))", 'CONFIG+=release', 'CONFIG-=debug_and_release'
        )
        Invoke-Checked (Join-Path $compilerBin 'mingw32-make.exe') @('-j2')
    } finally { Pop-Location }

    Copy-Item -LiteralPath (Join-Path $buildDir 'clangTest.exe') -Destination $appDir
    Write-Host 'Deploying Qt...'
    # DLL 运行库由后续扫描统一从 MSYS2 复制，避免混入旧版 libstdc++。
    Invoke-Checked (Join-Path $qtBin 'windeployqt.exe') @(
        '--release', '--no-translations', '--no-compiler-runtime', (Join-Path $appDir 'clangTest.exe')
    )
    Set-Content -LiteralPath (Join-Path $appDir 'qt.conf') -Encoding ascii -Value "[Paths]`nPrefix=.`nPlugins=."

    Write-Host 'Collecting DLL dependencies...'
    $pending = [System.Collections.Generic.Queue[string]]::new()
    $seen = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    Get-ChildItem -LiteralPath $appDir -Recurse -File |
        Where-Object { $_.Extension -in '.exe', '.dll' } |
        ForEach-Object { $pending.Enqueue($_.FullName) }
    while ($pending.Count -gt 0) {
        $binary = $pending.Dequeue()
        if (-not $seen.Add($binary)) { continue }
        $imports = @(& $objdump -p $binary 2>&1)
        if ($LASTEXITCODE -ne 0) { throw "Cannot inspect imports: $binary" }
        foreach ($line in $imports) {
            if ($line -notmatch 'DLL Name:\s*(\S+)') { continue }
            $dllName = $Matches[1]
            $destination = Join-Path $appDir $dllName
            if (Test-Path -LiteralPath $destination) {
                $pending.Enqueue($destination)
                continue
            }
            $source = @((Join-Path $clangBin $dllName), (Join-Path $qtBin $dllName)) |
                Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
            if ($source) {
                Copy-Item -LiteralPath $source -Destination $destination
                $pending.Enqueue($destination)
            } elseif ($dllName -match '^(api-ms-|ext-ms-)' -or
                      (Test-Path -LiteralPath (Join-Path "$env:SystemRoot/System32" $dllName))) {
                continue
            } else { throw "Unresolved DLL: $dllName (from $binary)" }
        }
    }

    Write-Host 'Bundling C/C++ headers...'
    $runtimeDir = Join-Path $appDir 'runtime'
    New-Item -ItemType Directory -Path $runtimeDir -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $ClangRoot 'include') -Destination $runtimeDir -Recurse
    $clangResources = Join-Path $runtimeDir 'lib/clang'
    New-Item -ItemType Directory -Path $clangResources -Force | Out-Null
    foreach ($versionDir in Get-ChildItem -LiteralPath (Join-Path $ClangRoot 'lib/clang') -Directory) {
        $resourceHeaders = Join-Path $versionDir.FullName 'include'
        if (Test-Path -LiteralPath $resourceHeaders) {
            $resourceTarget = Join-Path $clangResources $versionDir.Name
            New-Item -ItemType Directory -Path $resourceTarget -Force | Out-Null
            Copy-Item -LiteralPath $resourceHeaders -Destination $resourceTarget -Recurse
        }
    }
    $samplesDir = Join-Path $appDir 'samples'
    $sourceDir = Join-Path $appDir 'source'
    $licenseDir = Join-Path $appDir 'licenses'
    New-Item -ItemType Directory -Path $samplesDir, $sourceDir, $licenseDir -Force | Out-Null
    foreach ($sample in @('MsgHeader.h', 'SimpleMsgHeader.h', 'MinimalMsgHeader.h', 'PacketHeader.h', 'NestedPacketHeader.h')) {
        Copy-Item -LiteralPath (Join-Path $projectRoot $sample) -Destination $samplesDir
    }
    foreach ($file in @(
        'main.cpp', 'mainwindow.h', 'mainwindow.cpp', 'headerparser.h', 'headerparser.cpp', 'fieldvalue.h', 'fieldvalue.cpp', 'binaryencoder.h', 'binaryencoder.cpp', 'definitionjson.h', 'definitionjson.cpp',
        'MsgHeader.h', 'SimpleMsgHeader.h', 'MinimalMsgHeader.h', 'PacketHeader.h', 'NestedPacketHeader.h', 'clangTest.pro', 'README.md', 'package.ps1'
    )) {
        Copy-Item -LiteralPath (Join-Path $projectRoot $file) -Destination $sourceDir
    }
    Copy-Item -LiteralPath (Join-Path $projectRoot 'tests') -Destination $sourceDir -Recurse
    Copy-Item -LiteralPath (Join-Path $ClangRoot 'share/licenses') -Destination (Join-Path $licenseDir 'msys2') -Recurse
    Copy-Item -LiteralPath (Join-Path $QtRoot 'Licenses') -Destination (Join-Path $licenseDir 'qt') -Recurse

    $instructions = @'
clangTest Windows x64 便携版

使用方法
1. 完整解压此文件夹，然后双击 clangTest.exe。
2. 程序默认读取 samples/MsgHeader.h，显示 21 个字段、83 字节。
3. 点击“打开头文件…”选择 SimpleMsgHeader.h 或 MinimalMsgHeader.h 对比。
4. 修改并保存头文件后，点击“重新解析”。结构体名称输入框可指定查找的名称。
5. 打开 samples/PacketHeader.h，将结构体名称改为 PacketHeader，验证其他名称的解析。
6. 打开 samples/NestedPacketHeader.h，名称设为 NestedPacketHeader，展开 address 查看内部字段；也可使用“全部展开 / 全部折叠”。
7. 相对偏移表示相对直接所属结构体的位置，绝对偏移表示相对最外层结构体的位置。
8. 在“值”列填写基础类型字段，bool 使用下拉框。输入无效时标红，并保留上一次有效值；指针、引用和数组等暂时只读。
   整数可填十进制或带 0x / 0X 前缀的十六进制，例如 42、0x2A、-0x10，仍按字段类型范围校验。
9. 值仅保存在当前窗口内存中，关闭后不保留；同一结构体重新解析时保留仍符合类型要求的值。
10. 编辑后点击“查看二进制”查看小端序十六进制快照，并可复制字节；填充补 0。无效输入或不支持的字段会阻止生成。
    数组仍只读，二进制预览按实际大小全部填 0，允许含数组的结构体生成预览。
11. 点击“导出结构定义 JSON”保存当前结构体的类型、嵌套关系和布局信息，不含输入值。

自带 Qt、libclang、相关 DLL 和 C/C++ 头文件，无需安装 Qt 或 Clang。
请保留整个文件夹的目录结构。解析目标为 Windows x64 / MinGW，使用 C++11。

samples/  示例头文件
runtime/  解析所需的标准库及 Clang 内置头文件
source/   本程序源码和重新构建脚本
licenses/ 第三方组件随安装包提供的许可文件

第三方组件：Qt 5.12.8、MSYS2 Clang/libclang 23.1.4、LLVM、GCC 运行库等。
本程序动态链接 Qt，对应项目及构建说明位于 source/。
Qt 源代码：https://download.qt.io/archive/qt/5.12/5.12.8/
MSYS2 软件包源代码：https://github.com/msys2/MINGW-packages

GitHub 上传：将 clangTest-windows-x64.zip 作为 Release 附件上传。
'@
    Set-Content -LiteralPath (Join-Path $appDir '使用说明.txt') -Value $instructions -Encoding utf8
    Write-Host 'Compressing portable package...'
    Compress-Archive -LiteralPath $appDir -DestinationPath $zipPath -CompressionLevel Optimal -Force
    $digest = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
    Set-Content -LiteralPath ($zipPath + '.sha256') -Value "$digest  $([IO.Path]::GetFileName($zipPath))" -Encoding ascii
    $archive = Get-Item -LiteralPath $zipPath
    Write-Host ("Created: {0} ({1:N1} MB)" -f $archive.FullName, ($archive.Length / 1MB))
    Write-Host "Staging folder: $appDir"
    Write-Host "SHA256: $digest"
} finally { $env:PATH = $savedPath }
