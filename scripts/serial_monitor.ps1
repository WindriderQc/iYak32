param(
    [ValidatePattern('^COM[0-9]+$')][string]$Port = 'COM3',
    [ValidateRange(1200, 2000000)][int]$Baud = 115200,
    [ValidateRange(1, 3600)][int]$Seconds = 35
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)

function Write-SerialLine([string]$Line) {
    # Boot diagnostics can include private settings. Keep them out of captures.
    $Line = $Line -replace '(?i)("[^"\r\n]*(?:pass|secret|token|api.?key|mqttuser)[^"\r\n]*"\s*:\s*)"(?:\\.|[^"\\])*"', '$1"[MASKED]"'
    $Line = $Line -replace '(?i)((?:AP )?Password\s*:\s*).*', '$1[MASKED]'
    [Console]::WriteLine($Line)
}

$serial = New-Object System.IO.Ports.SerialPort($Port, $Baud, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$serial.DtrEnable = $false
$serial.RtsEnable = $false
$serial.Handshake = [System.IO.Ports.Handshake]::None
$serial.ReadTimeout = 200
$bytes = New-Object byte[] 4096
$chars = New-Object char[] 4096
$decoder = [System.Text.Encoding]::UTF8.GetDecoder()
$pending = ''

try {
    $serial.Open()
    [Console]::WriteLine("[SERIAL] READY $Port $Baud")
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt $Seconds) {
        try {
            # Read with a deadline; avoid BytesToRead/ClearCommError polling.
            $count = $serial.Read($bytes, 0, $bytes.Length)
        } catch [System.TimeoutException] {
            continue
        }
        if ($count -eq 0) { continue }
        $length = $decoder.GetChars($bytes, 0, $count, $chars, 0, $false)
        $pending += [string]::new($chars, 0, $length)
        while (($newline = $pending.IndexOf("`n")) -ge 0) {
            Write-SerialLine ($pending.Substring(0, $newline).TrimEnd("`r"))
            $pending = $pending.Substring($newline + 1)
        }
    }
    if ($pending.Length -gt 0) { Write-SerialLine $pending }
} finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
[Console]::WriteLine("[SERIAL] CLOSED $Port")
