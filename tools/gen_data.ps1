param(
    [string]$JsonPath = "$PSScriptRoot\..\data\flashcodes.json",
    [string]$OutPath  = "$PSScriptRoot\..\src\flashdata.cpp"
)

$ErrorActionPreference = 'Stop'

function ConvertTo-CppString([string]$text) {
    if ($null -eq $text) { return '' }
    $sb = New-Object System.Text.StringBuilder
    foreach ($ch in $text.ToCharArray()) {
        $code = [int][char]$ch
        switch ($ch) {
            '\' { [void]$sb.Append('\\'); continue }
            '"' { [void]$sb.Append('\"'); continue }
            "`n" { [void]$sb.Append('\n'); continue }
            "`r" { continue }
            "`t" { [void]$sb.Append('\t'); continue }
            default {
                if ($code -ge 32 -and $code -lt 127) {
                    [void]$sb.Append($ch)
                } else {
                    [void]$sb.Append(('\x{0:x2}' -f $code))
                }
            }
        }
    }
    return $sb.ToString()
}

$platforms = Get-Content -Raw -LiteralPath $JsonPath | ConvertFrom-Json

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('#include "flashdata.h"')
[void]$sb.AppendLine()

for ($p = 0; $p -lt $platforms.Count; $p++) {
    $platform = $platforms[$p]
    [void]$sb.AppendLine("static const FcOption fc_opts_$p[] = {")
    foreach ($option in $platform.options) {
        $digit = [int]$option.d
        $bit = [int]$option.b
        $code = ConvertTo-CppString $option.code
        $desc = ConvertTo-CppString $option.desc
        $note = ConvertTo-CppString $option.note
        [void]$sb.AppendLine("    { $digit, $bit, `"$code`", `"$desc`", `"$note`" },")
    }
    [void]$sb.AppendLine('};')
    [void]$sb.AppendLine()
}

[void]$sb.AppendLine('const FcPlatform g_platforms[] = {')
for ($p = 0; $p -lt $platforms.Count; $p++) {
    $platform = $platforms[$p]
    $id = ConvertTo-CppString $platform.id
    $name = ConvertTo-CppString $platform.name
    $supports24 = if ($platform.formats -match '24') { 'true' } else { 'false' }
    $count = @($platform.options).Count
    [void]$sb.AppendLine("    { `"$id`", `"$name`", $supports24, fc_opts_$p, $count },")
}
[void]$sb.AppendLine('};')
[void]$sb.AppendLine()
[void]$sb.AppendLine("const int g_platformCount = $($platforms.Count);")

Set-Content -LiteralPath $OutPath -Value $sb.ToString() -Encoding ascii
Write-Output "Wrote $OutPath"
