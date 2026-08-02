# GTAHUB-Addons - HUB-Core

Client-side ASI plugin cho SA-MP 0.3.DL / open.mp, viết bằng C++/Win32.

## Trạng thái hiện tại

Phần Custom D3D9 Nametag Engine, Role badge, RakNet role protocol và open.mp server role component đã được gỡ khỏi repository.

Code còn lại chỉ giữ ASI entrypoint tối giản:

- Khởi tạo log `HUB-Core.log`.
- Chờ SA-MP chat sẵn sàng.
- In thông báo version `[HUB-Core] HUBCore.asi Version: ...`.

## Cấu trúc còn lại

```text
HUB-Core/
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
