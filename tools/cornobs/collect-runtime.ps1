[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateRange(1, 2147483647)][int]$ObsProcessId,
    [Parameter(Mandatory)][ValidateRange(1, 2147483647)][int]$GameProcessId,
    [ValidateRange(1, 86400)][int]$DurationSeconds = 600,
    [ValidateRange(0.25, 60)][double]$IntervalSeconds = 1,
    [Parameter(Mandatory)][string]$OutputPath
)
$ErrorActionPreference = 'Stop'
# Read only process counters. Never read OBS settings, command lines or credentials.
$targets = @(
    @{ Role = 'obs'; Id = $ObsProcessId; PreviousCpu = $null; PreviousTime = 0.0; StartTicks = $null },
    @{ Role = 'game'; Id = $GameProcessId; PreviousCpu = $null; PreviousTime = 0.0; StartTicks = $null }
)
$absoluteOutput = [IO.Path]::GetFullPath($OutputPath)
$stream = [IO.File]::Open($absoluteOutput, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::Read)
$writer = New-Object IO.StreamWriter($stream, (New-Object Text.UTF8Encoding($true)))
$clock = [Diagnostics.Stopwatch]::StartNew()
$headerWritten = $false
try {
    do {
        foreach ($target in $targets) {
            $elapsed = $clock.Elapsed.TotalSeconds
            $cpu = $null
            $workingSet = $null
            $status = 'unavailable'
            try {
                $taskProc = Get-Process -Id $target.Id -ErrorAction Stop
                try {
                    $ticks = $taskProc.StartTime.ToUniversalTime().Ticks
                    if ($null -eq $target.StartTicks) { $target.StartTicks = $ticks }
                    if ($ticks -ne $target.StartTicks) { throw 'PID was reused' }
                    $totalCpu = $taskProc.TotalProcessorTime.TotalSeconds
                    if ($null -ne $target.PreviousCpu -and $elapsed -gt $target.PreviousTime) {
                        $cpu = 100.0 * ($totalCpu - $target.PreviousCpu) / ($elapsed - $target.PreviousTime)
                    }
                    $target.PreviousCpu = $totalCpu
                    $target.PreviousTime = $elapsed
                    $workingSet = $taskProc.WorkingSet64
                    $status = 'ok'
                } finally { $taskProc.Dispose() }
            } catch {
                $target.PreviousCpu = $null
            }
            $row = [pscustomobject]@{
                utc = [DateTime]::UtcNow.ToString('o')
                elapsed_seconds = $elapsed.ToString('F3', [Globalization.CultureInfo]::InvariantCulture)
                role = $target.Role
                process_id = $target.Id
                status = $status
                cpu_pct_one_core = $(if ($null -eq $cpu) { '' } else { $cpu.ToString('F3', [Globalization.CultureInfo]::InvariantCulture) })
                working_set_bytes = $workingSet
            }
            $csv = @($row | ConvertTo-Csv -NoTypeInformation)
            if (!$headerWritten) { $writer.WriteLine($csv[0]); $headerWritten = $true }
            $writer.WriteLine($csv[1])
        }
        $writer.Flush()
        $remaining = $DurationSeconds - $clock.Elapsed.TotalSeconds
        if ($remaining -gt 0) {
            Start-Sleep -Milliseconds ([int](1000 * [Math]::Min($IntervalSeconds, $remaining)))
        }
    } while ($clock.Elapsed.TotalSeconds -lt $DurationSeconds)
} finally {
    $writer.Dispose()
    $clock.Stop()
}
Write-Output "Saved process counters: $absoluteOutput"
