@echo off
:: Build fyjson64.dll — run from x64 Native Tools Command Prompt
cd /d "%~dp0src"
rc /nologo version.rc
cl /nologo /std:c++17 /O2 /LD /EHsc /DUNICODE /D_UNICODE ^
    dllmain.cpp yyjson.c version.res ^
    /Fe:..\fyjson64.dll ^
    /link /DEF:fyjson.def ^
    ole32.lib oleaut32.lib advapi32.lib comsuppw.lib
cd /d "%~dp0"
if exist fyjson64.dll (
    echo OK: fyjson64.dll
) else (
    echo FAILED
    exit /b 1
)
