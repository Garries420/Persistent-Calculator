@echo off
setlocal

if not exist build mkdir build
if not exist dist mkdir dist

cl /nologo /O2 /W4 /WX /TC /D_CRT_SECURE_NO_WARNINGS /MT ^
  tests\test_engine.c src\calc_engine.c ^
  /Fo:build\ /Fe:build\test_engine.exe
if errorlevel 1 exit /b 1
build\test_engine.exe
if errorlevel 1 exit /b 1

cl /nologo /O2 /W4 /WX /TC /D_CRT_SECURE_NO_WARNINGS /MT ^
  tests\test_updater.c src\update_parser.c ^
  /Fo:build\ /Fe:build\test_updater.exe
if errorlevel 1 exit /b 1
build\test_updater.exe
if errorlevel 1 exit /b 1

pushd assets
rc /nologo /fo ..\build\app.res app.rc
if errorlevel 1 (
  popd
  exit /b 1
)
popd

cl /nologo /O2 /W4 /WX /TC /D_CRT_SECURE_NO_WARNINGS /DUNICODE /D_UNICODE /MT ^
  src\main.c src\calc_engine.c src\updater.c src\update_parser.c build\app.res ^
  /Fo:build\ /Fe:dist\PersistentCalculator.exe ^
  /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib dwmapi.lib shell32.lib advapi32.lib winhttp.lib bcrypt.lib
if errorlevel 1 exit /b 1

echo Built dist\PersistentCalculator.exe
