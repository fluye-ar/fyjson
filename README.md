# fyjson

Fast JSON parser for COM — [yyjson](https://github.com/ibireme/yyjson) engine exposed via IDispatch. Native C++, x86/x64.

Drop-in replacement for aspJSON and similar VBScript JSON parsers. Same API simplicity, 1000x faster.

## Quick Start

```vbs
Set yyj = CreateObject("fyjson")

' Parse
Set obj = yyj.Parse("{""name"":""Juan"",""age"":30,""tags"":[""a"",""b""]}")
name = obj("name")              ' "Juan"
obj("name") = "Pedro"           ' modify
obj("city") = "Córdoba"         ' add

' Create from scratch
Set obj = yyj.NewObject()
obj("id") = 1
obj("tags") = Array("a","b","c")  ' VBS Array → JSON array

' Arrays
Set arr = yyj.NewArray()
arr.Add "uno"
arr.Add 2
arr.Add Null

' Iterate
For Each key In obj
    WScript.Echo key & " = " & obj(key)
Next

' Nest objects
Set inner = yyj.NewObject()
inner("x") = 1
Set obj("nested") = inner

' Serialize
WScript.Echo obj.ToString()
```

## API

**Factory** — `CreateObject("fyjson")`

| Method | Returns | Description |
|--------|---------|-------------|
| `Parse(jsonString)` | JsonObject | Parse JSON string |
| `NewObject()` | JsonObject | Empty `{}` |
| `NewArray()` | JsonObject | Empty `[]` |

**JsonObject** — objects, arrays and values

| Member | Type | Description |
|--------|------|-------------|
| `(key)` | r/w | Get/set by key (objects) |
| `(index)` | r/w | Get/set by index (arrays) |
| `Add(value)` | method | Append to array |
| `Remove(key\|index)` | method | Remove entry |
| `Count` | int | Number of keys/items |
| `Exists(key)` | bool | Key exists? |
| `IsObject` | bool | Is `{}`? |
| `IsArray` | bool | Is `[]`? |
| `IsNull` | bool | Is null? |
| `ToString()` | string | Serialize to JSON |
| `For Each` | enum | Iterate keys (objects) or values (arrays) |

## Benchmark

Parsing + iterating search results (FluyeDoors, 2026-04-22):

| Docs | JSON Size | aspJSON (VBS) | fyjson |
|------|-----------|---------------|--------|
| 100 | 14 KB | 125 ms | 0.1 ms |
| 1,000 | 142 KB | 7,941 ms | 0.3 ms |
| 15,000 | 5.4 MB | crash | 7.7 ms |

## Install

```bat
:: Register (run as admin)
%SystemRoot%\System32\regsvr32 fyjson64.dll
%SystemRoot%\SysWOW64\regsvr32 fyjson32.dll
```

## Build

Requires Visual Studio Build Tools with C++ and `/std:c++17`.

```bat
build64.bat
build32.bat
```

## Compatibility

Windows 7+ / Windows Server 2008 R2+. Works with any COM client: VBScript, VBA, Classic ASP, JScript, WSH, PowerShell.

## License

MIT — see [LICENSE](LICENSE).

yyjson engine by [ibireme](https://github.com/ibireme/yyjson) (MIT).

---
Jorge Pagano - Fluye
