if not exist out\build\x64-Debug mkdir out\build\x64-Debug
cd out\build\x64-Debug

cmake -S ../../../ -B . -G "MinGW Makefiles"
mingw32-make.exe && mingw32-make.exe Shaders
cd ../../..