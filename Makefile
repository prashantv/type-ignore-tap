
main: main.c
	cl /I ..\Interception\library main.c ..\Interception\library\x64\interception.lib

run: main
	main "HID\VID_05AC&PID_0262&REV_0222&MI_01&Col01" "HID\VID_05AC&PID_0262&REV_0222&MI_00&Col01"
