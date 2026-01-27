# get-rime-windows.ps1
# 下载 librime Windows 预编译库（x86 和 x64）

param (
    [string]$tag = "",
    [string]$proxy = ""
)

$ErrorActionPreference = "Stop"

# GitHub API URL
$apiUrl = if ($tag) {
    "https://api.github.com/repos/rime/librime/releases/tags/$tag"
} else {
    "https://api.github.com/repos/rime/librime/releases/latest"
}

# 目标目录
$depsDir = Join-Path $PSScriptRoot "..\deps\librime"
$libDir = Join-Path $depsDir "lib"
$lib32Dir = Join-Path $libDir "x86"
$lib64Dir = Join-Path $libDir "x64"
$includeDir = Join-Path $depsDir "include"

# 创建目录
New-Item -ItemType Directory -Force -Path $lib32Dir | Out-Null
New-Item -ItemType Directory -Force -Path $lib64Dir | Out-Null
New-Item -ItemType Directory -Force -Path $includeDir | Out-Null

Write-Host "Fetching release info from GitHub..."

$webParams = @{
    Uri = $apiUrl
    Headers = @{ Accept = "application/vnd.github.v3+json" }
}
if ($proxy) {
    $webParams.Proxy = $proxy
}

try {
    $response = Invoke-RestMethod @webParams
} catch {
    Write-Host "Error fetching release: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

Write-Host "Release: $($response.tag_name)"

# 匹配模式
$patternX86 = "rime-[0-9a-fA-F]+-Windows-msvc-x86\.7z"
$patternX64 = "rime-[0-9a-fA-F]+-Windows-msvc-x64\.7z"
$patternDepsX86 = "rime-deps-[0-9a-fA-F]+-Windows-msvc-x86\.7z"
$patternDepsX64 = "rime-deps-[0-9a-fA-F]+-Windows-msvc-x64\.7z"

$downloads = @()

foreach ($asset in $response.assets) {
    $url = $asset.browser_download_url
    $name = $asset.name
    
    if ($name -match $patternX86 -and $name -notmatch "deps") {
        $downloads += @{ url = $url; name = $name; type = "x86" }
    }
    elseif ($name -match $patternX64 -and $name -notmatch "deps") {
        $downloads += @{ url = $url; name = $name; type = "x64" }
    }
}

if ($downloads.Count -lt 2) {
    Write-Host "Could not find both x86 and x64 releases" -ForegroundColor Red
    exit 1
}

# 检查 7z
$has7z = $null -ne (Get-Command 7z -ErrorAction SilentlyContinue)
if (-not $has7z) {
    # 尝试常见的7z安装路径
    $sevenZipPaths = @(
        "C:\Program Files\7-Zip\7z.exe",
        "C:\Program Files (x86)\7-Zip\7z.exe",
        "$env:ProgramFiles\7-Zip\7z.exe"
    )
    foreach ($path in $sevenZipPaths) {
        if (Test-Path $path) {
            Set-Alias -Name 7z -Value $path -Scope Script
            $has7z = $true
            break
        }
    }
}
if (-not $has7z) {
    Write-Host "7z not found. Please install 7-Zip." -ForegroundColor Red
    Write-Host "Download from: https://www.7-zip.org/" -ForegroundColor Yellow
    exit 1
}

$tempDir = Join-Path $env:TEMP "rime-download"
New-Item -ItemType Directory -Force -Path $tempDir | Out-Null

foreach ($dl in $downloads) {
    $outFile = Join-Path $tempDir $dl.name
    $extractDir = Join-Path $tempDir ($dl.name -replace '\.7z$', '')
    
    Write-Host "Downloading $($dl.name)..."
    
    $dlParams = @{ Uri = $dl.url; OutFile = $outFile }
    if ($proxy) { $dlParams.Proxy = $proxy }
    
    Invoke-WebRequest @dlParams
    
    Write-Host "Extracting..."
    7z x $outFile -o"$extractDir" -y | Out-Null
    
    $distDir = Join-Path $extractDir "dist"
    $targetLibDir = if ($dl.type -eq "x86") { $lib32Dir } else { $lib64Dir }
    
    # 复制库文件
    Copy-Item (Join-Path $distDir "lib\rime.dll") $targetLibDir -Force
    Copy-Item (Join-Path $distDir "lib\rime.lib") $targetLibDir -Force
    if (Test-Path (Join-Path $distDir "lib\rime.pdb")) {
        Copy-Item (Join-Path $distDir "lib\rime.pdb") $targetLibDir -Force
    }
    
    # 复制头文件（只需要一次）
    if ($dl.type -eq "x64") {
        Copy-Item (Join-Path $distDir "include\rime_*.h") $includeDir -Force
    }
    
    Write-Host "$($dl.type) libraries copied to $targetLibDir" -ForegroundColor Green
}

# 清理
Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "Done! Libraries installed to:" -ForegroundColor Green
Write-Host "  x86: $lib32Dir"
Write-Host "  x64: $lib64Dir"
Write-Host "  Headers: $includeDir"
