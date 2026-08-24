# GTAHUB-Addons - HUB-Core

Client-side ASI plugin cho SA-MP 0.3.DL / open.mp, viết bằng C++/Win32.

## Trạng thái hiện tại

Client hiện gồm:

- Custom D3D9 Nametag Engine và role badge.
- RakNet role protocol kết nối với open.mp server component.
- Multi-slot role, preset badges, image badges, rainbow effects và nametag custom color.
- Direct3D9 lost/reset lifecycle.

## Cấu trúc chính

```text
HUB-Core/
├── D3DHook.cpp
├── Nametag.cpp
├── Network.cpp
├── dllmain.cpp
├── framework.h
├── pch.cpp
├── pch.h
└── HUB-Core.vcxproj
```

## Build

Yêu cầu:

- Visual Studio 2019+.
- Windows SDK 10.0.
- `sampapi.lib` có sẵn trong `HUB-Core/`.

Build bằng Visual Studio:

1. Mở `HUB-Core.sln`.
2. Chọn `Release | Win32`.
3. Build solution.

Hoặc dùng MSBuild:

```cmd
MSBuild.exe HUB-Core\HUB-Core.vcxproj /p:Configuration=Release /p:Platform=Win32
```
