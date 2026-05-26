#get blth audio
Get-PnpDevice -Class "Bluetooth" -PresentOnly | Where-Object { 
    $properties = Get-PnpDeviceProperty -InstanceId $_.InstanceId -KeyName "DEVPKEY_Device_IsConnected"
    $null -ne $properties -and $properties.Data -eq $true 
} | Select-Object FriendlyName, Status, InstanceId