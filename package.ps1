param([string]$OutputDirectory = (Join-Path $PSScriptRoot 'build\package'))
$ErrorActionPreference = 'Stop'
$version = (Select-String -LiteralPath (Join-Path $PSScriptRoot 'src\game\Version.h') -Pattern '#define KEK_VERSION_STR "([^"]+)"').Matches[0].Groups[1].Value
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
$stage = Join-Path $outputPath "v$version\KekMeIfYouCan"
$videoDir = Join-Path $stage 'data\video'
New-Item -ItemType Directory -Force -Path $videoDir | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'KekMeIfYouCan-Ordner.exe') -Destination (Join-Path $stage 'KekMeIfYouCan.exe') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'KekMeIfYouCanCLI.exe') -Destination $stage -Force
$files = @('KekMeIfYouCan.exe', 'KekMeIfYouCanCLI.exe', 'folder-install.txt', 'LIESMICH.txt')
foreach ($video in @('kekishintro.mp4', 'engineintro.mp4', 'spielintro.mp4')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "res\video\$video") -Destination $videoDir -Force
    $files += "data/video/$video"
}
$utf8 = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $stage 'folder-install.txt'), "Kek Me If You Can - Ordner-Ausgabe`r`nDiesen Marker fuer passende automatische EXE-Updates im Spielordner lassen.`r`n", $utf8)
[IO.File]::WriteAllText((Join-Path $stage 'LIESMICH.txt'), @"
KEK ME IF YOU CAN $version - Windows x64

1. Das gesamte ZIP entpacken (nicht direkt im ZIP starten).
2. Im entpackten Ordner KekMeIfYouCan.exe starten.
3. data/video und folder-install.txt neben der EXE behalten.

Enthalten: Spiel, Konsolen-/Serverprogramm und alle drei Intro-Videos.
Karten, Figuren, Schrift und Sounds werden von der Engine erzeugt oder
nutzen Windows-Komponenten. Es fehlen keine zusaetzlichen Asset-Pakete.
Kein Installer und keine Administratorrechte erforderlich.

Vier Karten: Bundestag, Oktoberfest, Nudelfabrik, KEK-Paketzentrum.
Spezialmodi: Prop-Roulette, Klon-Alarm, Hummeln im Hintern, Mondschicht.
Wundertuete waehlt pro Runde einen anderen Spezialmodus.
Der Gastgeber waehlt vor dem Start oder per Esc fuer die naechste Runde.

Der automatische Updater aktualisiert beide EXEs. Bei neuen Videodateien
das aktuelle vollstaendige ZIP entpacken. Eigene Einstellungen stehen in
prophunt.ini; diese Datei wird vom Paket nicht ueberschrieben.

Diese Ausgabe ist nicht digital signiert. Eine Windows-Warnung wegen
unbekanntem Herausgeber kann deshalb weiterhin erscheinen. Bei einem
konkreten Virenfund die genaue Meldung pruefen und dem Projekt melden.
SHA256SUMS.txt enthaelt Pruefsummen zum Vergleichen der Dateien; sie sind
kein Sicherheitszertifikat und ersetzen keine Virenpruefung.

Downloads und Quellcode: https://github.com/GG2EZBoOm/kek-me-if-u-can
"@, $utf8)
$sums = foreach ($relative in $files) {
    $hash = (Get-FileHash -LiteralPath (Join-Path $stage $relative) -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $relative"
}
[IO.File]::WriteAllText((Join-Path $stage 'SHA256SUMS.txt'), ($sums -join "`n") + "`n", $utf8)
$files += 'SHA256SUMS.txt'

# Exakt die Paketdateien archivieren, keine Logs oder lokalen Einstellungen.
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zipPath = Join-Path $outputPath 'KekMeIfYouCan-Windows.zip'
$stream = [IO.File]::Open($zipPath, [IO.FileMode]::Create)
try {
    $zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create, $true)
    try {
        foreach ($relative in $files) {
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, (Join-Path $stage $relative), "KekMeIfYouCan/$relative", [IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
    } finally { $zip.Dispose() }
} finally { $stream.Dispose() }
$releaseSums = foreach ($path in @((Join-Path $PSScriptRoot 'KekMeIfYouCan.exe'), (Join-Path $PSScriptRoot 'KekMeIfYouCan-Ordner.exe'), (Join-Path $PSScriptRoot 'KekMeIfYouCanCLI.exe'), $zipPath)) {
    $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $([IO.Path]::GetFileName($path))"
}
[IO.File]::WriteAllText((Join-Path $outputPath 'SHA256SUMS.txt'), ($releaseSums -join "`n") + "`n", $utf8)
Write-Output "Spielordner: $stage"
Write-Output "Download-ZIP: $zipPath"
