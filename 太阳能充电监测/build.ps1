param([string]$Toolchain='F:\Keil\core\ARM\ARMCLANG\bin',[switch]$Teaching,[switch]$Oled)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$src=if($Teaching){Join-Path $root 'Teaching\Src'}else{Join-Path $root 'Src'}
$out=if($Teaching){Join-Path $root 'Build-Teaching'}else{Join-Path $root 'Build'}
if($Oled){$out+='-OLED'}
$screenDefine=if($Oled){'-DDISPLAY_TFT=0'}else{'-DBUILD_SCRIPT=1'}
New-Item -ItemType Directory -Force -Path $out | Out-Null
$objects=@()
foreach($file in Get-ChildItem -LiteralPath $src -Filter '*.c') {
    $obj=Join-Path $out ($file.BaseName+'.o')
    & "$Toolchain\armclang.exe" --target=arm-arm-none-eabi -mcpu=cortex-m0plus -mthumb -std=c11 -O1 -g -Wall -Wextra -Werror -Wconversion -Wmissing-prototypes -Wmissing-variable-declarations -DSTM32G031xx $screenDefine -I "$root\Inc" -I "$root\Vendor" -c $file.FullName -o $obj
    if($LASTEXITCODE -ne 0){throw "Compile failed: $($file.Name)"};$objects+=$obj
}
& "$Toolchain\armasm.exe" --cpu Cortex-M0plus --apcs=interwork --pd '__MICROLIB SETA 1' "$root\Vendor\startup_stm32g031xx.s" -o "$out\startup.o"
if($LASTEXITCODE -ne 0){throw 'Startup failed'}
$objects+="$out\startup.o"
& "$Toolchain\armlink.exe" --cpu Cortex-M0plus --library_type=microlib --strict --scatter "$root\Keil\memory.sct" --map --info sizes,totals --list "$out\firmware.map" $objects -o "$out\firmware.axf"
if($LASTEXITCODE -ne 0){throw 'Link failed'}
& "$Toolchain\fromelf.exe" --i32combined --output "$out\firmware.hex" "$out\firmware.axf"
if($LASTEXITCODE -ne 0){throw 'Hex failed'}
& "$Toolchain\fromelf.exe" --bin --output "$out\firmware.bin" "$out\firmware.axf"
if($LASTEXITCODE -ne 0){throw 'Bin failed'}


