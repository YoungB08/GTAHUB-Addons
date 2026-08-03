# GTAHUB-Addons - HUB-Core

Client-side ASI plugin cho SA-MP 0.3.DL / open.mp, viết bằng C++/Win32.

## Trạng thái hiện tại

Client hiện gồm:

- Custom D3D9 Nametag Engine và role badge.
- RakNet role protocol kết nối với open.mp server component.
- Custom Chat cho SA-MP 0.3.DL R1 với native fallback.
- Chat capture tại điểm hội tụ `CChat::AddEntry` và custom network packet, gửi qua original `CInput::Send`.
- Composer UTF-16, Windows IME, clipboard Unicode, selection, history và smooth scroll.
- Direct3D9 lost/reset lifecycle và runtime config tại `HUB-Core/chat.ini`.

## Cấu trúc chính

```text
HUB-Core/
├── CustomChat.cpp
├── CustomChatInput.cpp
├── ChatManager.cpp
├── ChatSettings.cpp
├── HookManager.cpp
├── D3DHook.cpp
├── Nametag.cpp
├── Network.cpp
├── dllmain.cpp
├── framework.h
├── pch.cpp
├── pch.h
├── chat.ini
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

Custom Chat chỉ cài internal hooks khi phát hiện SA-MP 0.3.DL R1. Nếu hook hoặc D3D resource không sẵn sàng, native chat tiếp tục render.
