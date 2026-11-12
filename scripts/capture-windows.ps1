[CmdletBinding()]
param(
    [ValidateRange(10, 120)]
    [int]$Seconds = 30,
    [ValidateRange(0, 999)]
    [int]$Interface = 0,
    [string]$OutputDirectory = [Environment]::GetFolderPath('Desktop')
)

$ErrorActionPreference = 'Stop'
$dumpcapCommand = Get-Command dumpcap.exe -ErrorAction SilentlyContinue
if ($dumpcapCommand) {
    $dumpcap = $dumpcapCommand.Source
} else {
    $dumpcap = Join-Path $env:ProgramFiles 'Wireshark\dumpcap.exe'
}
if (-not (Test-Path -LiteralPath $dumpcap)) {
    throw 'Install Wireshark with Npcap, then run this script again. https://www.wireshark.org/download.html'
}

$interfaces = @(& $dumpcap -D 2>&1)
if ($LASTEXITCODE -ne 0) {
    throw "Cannot enumerate capture interfaces. Check Npcap permissions. $($interfaces -join [Environment]::NewLine)"
}
if ($Interface -eq 0) {
    $loopback = @($interfaces | Where-Object { "$_" -match '(?i)(NPF_Loopback|loopback traffic)' })
    if ($loopback.Count -ne 1 -or "$($loopback[0])" -notmatch '^\s*(\d+)\.') {
        $interfaces | ForEach-Object { Write-Host $_ }
        throw 'Cannot select the Npcap loopback adapter automatically. Run again with -Interface NUMBER from this list.'
    }
    $Interface = [int]$Matches[1]
}

if (-not (Test-Path -LiteralPath $OutputDirectory -PathType Container)) {
    throw 'OutputDirectory must be an existing directory.'
}
$file = Join-Path $OutputDirectory ('xdisplay-usbmux-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.pcapng')
Write-Host "Capturing loopback TCP port 27015 for $Seconds seconds."
Write-Host 'NOW start the Windows XDisplay agent, wait for the iPad image, move a window, then test one and two fingers.'
Write-Host "Output: $file"
& $dumpcap -i "$Interface" -f 'tcp port 27015' -a "duration:$Seconds" -w $file
if ($LASTEXITCODE -ne 0) {
    throw "Capture failed (exit $LASTEXITCODE)."
}
if ((Get-Item -LiteralPath $file).Length -lt 1000) {
    Write-Warning 'Capture is very small. Check adapter, restart the agent during capture, and verify the iPad displays an image.'
}
Write-Host "Capture complete: $file"
