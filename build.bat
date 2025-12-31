
cmake -B build

REM Build Release
cmake --build build --config Release

echo.
echo Build complete! Executable at: build\Release\flex.exe
