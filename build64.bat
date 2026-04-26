@echo off
:: Build yyjson64.dll — run from x64 Native Tools Command Prompt
cd /d "%~dp0src"
rc /nologo version.rc
cl /nologo /std:c++17 /O2 /LD /EHsc /DUNICODE /D_UNICODE ^
    dllmain.cpp yyjson.c version.res ^
    /Fe:..\yyjson64.dll ^
    /link /DEF:yyjson-com.def ^
    ole32.lib oleaut32.lib advapi32.lib comsuppw.lib
cd /d "%~dp0"
if exist yyjson64.dll (
    echo OK: yyjson64.dll
) else (
    echo FAILED
    exit /b 1
)
