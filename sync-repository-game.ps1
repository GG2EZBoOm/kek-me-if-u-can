param([Parameter(Mandatory = $true)][string]$SourceDirectory)
$ErrorActionPreference = 'Stop'

# Der normale GitHub-Download soll ohne Compiler spielbar sein. Nur geprüfte
# Paketdateien übernehmen; persönliche Einstellungen und Logs bleiben unberührt.
$sourcePath = [IO.Path]::GetFullPath($SourceDirectory)
$files = @('KekMeIfYouCan.exe', 'KekMeIfYouCanCLI.exe', 'folder-install.txt',
    'LIESMICH.txt', 'CREDITS.txt', 'ZapSplat-EULA.pdf', 'sources.json',
    'data/video/kekishintro.mp4', 'data/video/engineintro.mp4', 'data/video/spielintro.mp4')
$manifest = Join-Path $sourcePath 'SHA256SUMS.txt'
$expected = @{}
foreach ($line in Get-Content -LiteralPath $manifest) {
    $parts = $line -split '  ', 2
    if ($parts.Count -ne 2 -or $parts[0] -notmatch '^[a-fA-F0-9]{64}$' -or
        $parts[1] -notin $files -or $expected.ContainsKey($parts[1])) {
        throw 'Das Paket enthält eine ungültige Prüfsummenliste.'
    }
    $expected[$parts[1]] = $parts[0]
}
if ($expected.Count -ne $files.Count) { throw 'Im Paket fehlen Prüfsummen.' }
foreach ($relative in $files) {
    $hash = (Get-FileHash -LiteralPath (Join-Path $sourcePath $relative) -Algorithm SHA256).Hash
    if ($hash -ne $expected[$relative]) { throw "Beschädigte Paketdatei: $relative" }
}
$version = (Select-String -LiteralPath (Join-Path $PSScriptRoot 'src/game/Version.h') -Pattern '#define KEK_VERSION_STR "([^"]+)"').Matches[0].Groups[1].Value
foreach ($exe in @('KekMeIfYouCan.exe', 'KekMeIfYouCanCLI.exe')) {
    if ((Get-Item -LiteralPath (Join-Path $sourcePath $exe)).VersionInfo.FileVersion -ne $version) {
        throw "Die Programmversion passt nicht zu Version.h: $exe"
    }
}
foreach ($relative in ($files + 'SHA256SUMS.txt')) {
    $destination = Join-Path $PSScriptRoot $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $sourcePath $relative) -Destination $destination -Force
}
Write-Output "GitHub-Download enthält jetzt den vollständigen Spielordner $version."
