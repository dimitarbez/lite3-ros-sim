param(
    [string]$LocalAddress = "",
    [int]$LocalPort = 43897,
    [string]$TargetAddress = "192.168.2.1",
    [int]$TargetPort = 43893
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($LocalAddress)) {
    $candidates = @(Get-NetIPAddress -AddressFamily IPv4 |
        Where-Object {
            $_.IPAddress -like "192.168.2.*" -and
            $_.IPAddress -ne $TargetAddress -and
            $_.IPAddress -ne "192.168.2.196"
        } |
        Sort-Object -Property InterfaceMetric)
    if ($candidates.Count -ne 1) {
        throw "Expected exactly one Windows address on the robot Wi-Fi subnet; found $($candidates.Count). Set HARDWARE_WIFI_IP explicitly."
    }
    $LocalAddress = $candidates[0].IPAddress
}

$sourceAddress = [System.Net.IPAddress]::Parse($LocalAddress)
$localEndpoint = [System.Net.IPEndPoint]::new($sourceAddress, $LocalPort)
$motionAddress = [System.Net.IPAddress]::Parse($TargetAddress)
$targetEndpoint = [System.Net.IPEndPoint]::new($motionAddress, $TargetPort)
$client = [System.Net.Sockets.UdpClient]::new($localEndpoint)
$yawCode = [uint32]0x21010135
$heightCode = [uint32]0x21010102
$watchdogEnabled = $false

function Send-One([uint32]$Code, [int32]$Value) {
    $packet = [byte[]]::new(12)
    [System.BitConverter]::GetBytes($Code).CopyTo($packet, 0)
    [System.BitConverter]::GetBytes($Value).CopyTo($packet, 4)
    [System.BitConverter]::GetBytes([uint32]0).CopyTo($packet, 8)
    [void]$client.Send($packet, $packet.Length, $targetEndpoint)
}

function Send-Neutral {
    for ($attempt = 0; $attempt -lt 5; $attempt++) {
        Send-One $yawCode 0
        Send-One $heightCode 0
        Start-Sleep -Milliseconds 20
    }
    [Console]::Out.WriteLine("ACK ZERO")
    [Console]::Out.Flush()
}

try {
    [Console]::Out.WriteLine(("READY {0}:{1}" -f $LocalAddress, $LocalPort))
    [Console]::Out.Flush()
    $readTask = [Console]::In.ReadLineAsync()
    while ($true) {
        if ($watchdogEnabled -and -not $readTask.Wait(250)) {
            Send-Neutral
            throw "Pose input watchdog expired after 250 ms."
        }
        $line = $readTask.GetAwaiter().GetResult()
        if ($null -eq $line) {
            break
        }
        $parts = $line.Trim().Split(" ", [System.StringSplitOptions]::RemoveEmptyEntries)
        if ($parts.Length -eq 2 -and $parts[0] -eq "WATCHDOG" -and $parts[1] -eq "ON") {
            $watchdogEnabled = $true
        }
        elseif ($parts.Length -eq 2 -and $parts[0] -eq "WATCHDOG" -and $parts[1] -eq "OFF") {
            $watchdogEnabled = $false
        }
        elseif ($parts.Length -eq 1 -and $parts[0] -eq "ZERO") {
            Send-Neutral
        }
        elseif ($parts.Length -eq 3 -and $parts[0] -eq "ONE") {
            $code = [uint32]::Parse($parts[1], [System.Globalization.CultureInfo]::InvariantCulture)
            $value = [int32]::Parse($parts[2], [System.Globalization.CultureInfo]::InvariantCulture)
            Send-One $code $value
        }
        elseif ($parts.Length -eq 3 -and $parts[0] -eq "PAIR") {
            $yaw = [int32]::Parse($parts[1], [System.Globalization.CultureInfo]::InvariantCulture)
            $height = [int32]::Parse($parts[2], [System.Globalization.CultureInfo]::InvariantCulture)
            Send-One $yawCode $yaw
            Send-One $heightCode $height
        }
        else {
            throw "Expected WATCHDOG ON/OFF, ZERO, ONE CODE VALUE, or PAIR YAW HEIGHT"
        }
        $readTask = [Console]::In.ReadLineAsync()
    }
}
finally {
    try { Send-Neutral } catch {}
    $client.Close()
}
