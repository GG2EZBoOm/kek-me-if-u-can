# Asset preparation with Windows PowerShell and an installed German desktop voice.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Speech
$voice = New-Object System.Speech.Synthesis.SpeechSynthesizer
$pcm = [IO.MemoryStream]::new()
try {
    $speaker = $voice.GetInstalledVoices() | Where-Object { $_.Enabled -and $_.VoiceInfo.Culture.Name -eq 'de-DE' } | Select-Object -First 1
    if (!$speaker) { throw 'Eine deutsche Desktop-Stimme wird zum Erzeugen der Ansage benötigt.' }
    $voice.SelectVoice($speaker.VoiceInfo.Name)
    $format = [System.Speech.AudioFormat.SpeechAudioFormatInfo]::new(44100, 'Sixteen', 'Mono')
    $voice.SetOutputToAudioStream($pcm, $format)
    $voice.Speak('Das Mikrofon hat um das Wort gebeten.')
    $samples = $pcm.ToArray()
} finally { $voice.Dispose(); $pcm.Dispose() }
if (!$samples.Length -or $samples.Length -gt 44100*2*5) { throw 'Ungültige Sprachaufnahme.' }
$outFile = Join-Path $PSScriptRoot '../res/audio/duty-voice.bank'
$writer = [IO.BinaryWriter]::new([IO.File]::Create($outFile))
try {
    $writer.Write([Text.Encoding]::ASCII.GetBytes('KSB1'))
    foreach ($n in @(44100, 1, 4, ($samples.Length/2))) { $writer.Write([int]$n) }
    $writer.Write([Text.Encoding]::ASCII.GetBytes('word'))
    $writer.Write($samples)
} finally { $writer.Dispose() }
