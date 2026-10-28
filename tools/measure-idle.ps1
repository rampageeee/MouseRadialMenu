param([Parameter(Mandatory=$true)][int]$RadialProcessId, [int]$Seconds = 60)
$ErrorActionPreference = "Stop"
if ($Seconds -lt 10) { throw "Use at least a 10 second controlled interval" }
Write-Host "Keep the menu closed and leave the app idle. Warming up for 10 seconds."
Start-Sleep -Seconds 10
$Process = Get-Process -Id $RadialProcessId
if ($Process.ProcessName -ne "TinyRadial") { throw "The selected PID is not TinyRadial" }
$InitialCpu = $Process.TotalProcessorTime.TotalSeconds
$PrivateSamples = @($Process.PrivateMemorySize64)
$Watch = [System.Diagnostics.Stopwatch]::StartNew()
while ($Watch.Elapsed.TotalSeconds -lt $Seconds) {
    Start-Sleep -Seconds 1
    $Process.Refresh()
    if ($Process.HasExited) { throw "TinyRadial exited during measurement" }
    $PrivateSamples += $Process.PrivateMemorySize64
}
$Watch.Stop()
$CpuSeconds = $Process.TotalProcessorTime.TotalSeconds - $InitialCpu
$Path = $Process.Path
[pscustomobject]@{
    TimestampUtc = [DateTime]::UtcNow.ToString("o")
    Windows = [Environment]::OSVersion.VersionString
    LogicalProcessors = [Environment]::ProcessorCount
    ExecutableBytes = (Get-Item $Path).Length
    ExecutableSHA256 = (Get-FileHash $Path -Algorithm SHA256).Hash
    ResidentTinyRadialProcesses = @(Get-Process -Name TinyRadial).Count
    IntervalSeconds = $Watch.Elapsed.TotalSeconds
    CpuSeconds = $CpuSeconds
    CpuPercentOneCore = 100 * $CpuSeconds / $Watch.Elapsed.TotalSeconds
    PrivateBytesMean = ($PrivateSamples | Measure-Object -Average).Average
    PrivateBytesMaximum = ($PrivateSamples | Measure-Object -Maximum).Maximum
    Note = "Idle only. Startup and hotkey-to-visible latency require separate measurement."
} | Format-List
