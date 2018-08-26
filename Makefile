
main: main.c
	cl /I ..\Interception\library main.c ..\Interception\library\x64\interception.lib

run: main
	main