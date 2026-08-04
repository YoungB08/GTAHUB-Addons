# KẾ HOẠCH TRIỂN KHAI CUSTOM CHAT

**Trạng thái:** MVP ĐÃ TRIỂN KHAI - CHỜ KIỂM THỬ IN-GAME
**Branch:** `feature/custom-chat-system`  
**Tài liệu nguồn:** Phần SRS bên dưới được giữ nguyên để đối chiếu yêu cầu.

---

## 1. Mục tiêu bản đầu tiên

Xây dựng Custom Chat production-ready cho SA-MP 0.3.DL R1 trên Win32/x86, tích hợp vào hạ tầng `HUB-Core` hiện tại và thay thế phần hiển thị/nhập liệu chat mặc định mà không thay đổi protocol của SA-MP.

MVP bắt buộc gồm:

- Nhận đầy đủ server chat, client/system message và giữ nguyên màu SA-MP gửi xuống.
- Render cửa sổ chat, timestamp, prefix, word wrap, scroll và composer bằng Direct3D9.
- Mở chat bằng luồng `CInput::Open`, đóng bằng Enter/ESC và gửi qua `CInput::Send` gốc.
- Buffer UTF-16, IME tiếng Việt, clipboard Unicode, caret, selection và history.
- Ẩn native chat/input khi Custom Chat đã sẵn sàng; tự quay về native UI nếu khởi tạo hoặc hook thất bại.
- Xử lý đúng Direct3D9 lost/reset device và không làm hỏng render state của game/nametag.
- Cấu hình runtime cho kích thước, scale, font size, opacity, timestamp, fade và smooth scroll.
- Có log lỗi, giới hạn bộ nhớ, cache layout và số liệu debug để kiểm chứng hiệu năng.

Không đưa vào MVP:

- Search, filter UI, export log, hyperlink interaction, mention sound.
- Autocomplete command, emoji picker, reaction, reply, pin, tab/channel UI.
- Hỗ trợ phiên bản SA-MP khác 0.3.DL R1 hoặc open.mp client khác ABI.
- Custom RakNet protocol cho chat chuẩn của SA-MP.

---

## 2. Hiện trạng repository

- `ChatManager` đã có queue cơ bản nhưng trả về bản sao toàn bộ message và chưa có model Unicode/layout cache.
- `HookManager` đã khai báo offset cho `CChat::AddChatMessage`, `CChat::AddMessage` và các hàm `CInput`, nhưng chat hook đang bị bỏ qua và input hook mới chỉ được tạo, chưa enable.
- `D3DHook` đã hook `Reset`/`Present` và đang render nametag; Custom Chat phải dùng chung lifecycle này.
- `Network` có packet chat riêng 228-232 nhưng hiện chỉ log; packet này không thay thế được standard SA-MP chat.
- Chưa có WndProc hook, IME manager, clipboard Unicode, renderer chat, config parser hoặc test target.
- `HUB-Core/CustomChat.cpp` không tồn tại trong branch hiện tại; chức năng sẽ được chia module thay vì tạo một file lớn.

---

## 3. Quyết định kiến trúc đề xuất

### 3.1 Message capture

- Hook đúng hai điểm vào `CChat::AddChatMessage` và `CChat::AddMessage` bằng MinHook.
- Copy dữ liệu vào model sở hữu bộ nhớ riêng trước khi gọi original function.
- Luôn gọi original function để giữ nguyên logging, nội bộ SA-MP và compatibility.
- Không đọc `CChat::m_entry`, không polling, không scan history và không tự tạo RakNet packet.
- Nếu một hook không cài được, vô hiệu hóa Custom Chat và giữ native chat hoạt động.

### 3.2 Ẩn native chat

Yêu cầu "ẩn native chat" xung đột với đồng thời cấm hook render, cấm patch byte và cấm ghi `CChat::m_nMode`. Không có đường an toàn để bảo đảm cả bốn điều cùng lúc.

Đề xuất ngoại lệ tối thiểu:

- Hook `CChat::Render` chỉ để bỏ qua phần vẽ native chat khi Custom Chat ở trạng thái `Ready`.
- Không sửa byte, không sửa struct và không chặn logic nhận/lưu message.
- Khi Custom Chat lỗi, device lost chưa phục hồi hoặc hook không đầy đủ, gọi original `CChat::Render` ngay.
- Không hook `RenderEntry`, `Draw`, `PageUp` hoặc `PageDown`.

Ngoại lệ này phải được duyệt trước khi code.

### 3.3 Input và gửi message

- Dùng `CInput::Open`/`Close` làm tín hiệu đồng bộ trạng thái mở chat với SA-MP.
- Subclass game window bằng `SetWindowLongPtr(GWLP_WNDPROC)` và luôn chuyển message chưa xử lý qua `CallWindowProc`.
- Xử lý `WM_CHAR`, `WM_KEYDOWN`, `WM_IME_*`, clipboard và mouse trong buffer `std::wstring`.
- Chỉ convert UTF-16 sang encoding SA-MP cần một lần khi nhấn Enter, sau đó gọi original `CInput::Send`.
- Command bắt đầu bằng `/` được gửi nguyên văn; không parse hoặc bypass `CInput::Send`.
- Thực hiện spike runtime trước để xác định cách vô hiệu phần editbox mặc định mà vẫn giữ cursor/game-control state. Chỉ dùng hook hẹp nhất đã kiểm chứng; không đọc/ghi `m_szInput`.

### 3.4 Rendering và resource

- Thêm facade `CustomChatSystem` với lifecycle `Initialize`, `Render`, `OnLostDevice`, `OnResetDevice`, `Shutdown`.
- Gọi render chat từ `D3DHook::HookedPresent` cùng nametag, có state block để phục hồi D3D state.
- Resource do một `ChatResourceManager` sở hữu; không tạo/release resource trong render loop.
- MVP dùng `ID3DXFont` với Tahoma cho Unicode tiếng Việt. Ký tự thiếu glyph phải thay thế an toàn, không crash; font fallback nhiều họ và emoji màu để phase sau.
- Layout được tính theo viewport/scale và metrics tập trung, không rải magic number.

### 3.5 Config

- Dùng `HUB-Core/chat.ini` và Win32 profile API để tránh thêm dependency JSON vào DLL x86.
- Parse vào snapshot immutable, validate/clamp toàn bộ giá trị rồi swap tại frame boundary.
- Reload bằng timestamp file theo chu kỳ thấp hoặc phím debug, không đọc file mỗi frame.
- Giá trị lỗi dùng default và ghi warning; không làm Custom Chat crash.

### 3.6 Ownership và thread model

- Hook capture có thể đẩy message từ game/network path vào queue giới hạn.
- D3D thread drain queue và cập nhật model/layout trước khi render.
- Không giữ mutex trong lúc gọi SA-MP original function hoặc Direct3D API.
- Không cấp phát trong vòng draw nóng; allocation chỉ xảy ra khi có message/config/layout invalidation.

---

## 4. Cấu trúc file dự kiến

```text
HUB-Core/
├── CustomChat/
│   ├── CustomChatSystem.h/.cpp
│   ├── ChatMessage.h
│   ├── ChatMessageStore.h/.cpp
│   ├── ChatInput.h/.cpp
│   ├── ChatIme.h/.cpp
│   ├── ChatClipboard.h/.cpp
│   ├── ChatLayout.h/.cpp
│   ├── ChatRenderer.h/.cpp
│   ├── ChatResources.h/.cpp
│   ├── ChatSettings.h/.cpp
│   └── ChatMetrics.h
├── ChatManager.h/.cpp
├── HookManager.h/.cpp
├── D3DHook.h/.cpp
└── chat.ini
```

Không tách module chỉ để đạt số lượng file. Nếu IME/clipboard/layout đủ nhỏ và không tạo coupling, có thể gộp vào module input tương ứng khi triển khai.

---

## 5. Các phase triển khai

### Phase 0 - Baseline và proof-of-viability

- Build `Release | Win32` từ branch mới và ghi lại warning/error hiện tại.
- Thêm log xác nhận ABI/offset và kiểm tra địa chỉ thuộc vùng executable của `samp.dll`.
- Bật từng hook capture/input trong môi trường game để xác minh calling convention và lifecycle.
- Kiểm chứng cơ chế ẩn native chat/input cùng fallback trước khi xây renderer đầy đủ.

**Gate:** Không qua phase này nếu có crash, sai stack/calling convention hoặc native fallback không hoạt động.

### Phase 1 - Core model và settings

- Chuẩn hóa `ChatMessage`, ID, timestamp, prefix/text color và message source/type.
- Thay API copy toàn queue bằng snapshot/range visible hoặc drain queue có giới hạn.
- Thêm message cap, input history cap và policy clear/reset.
- Tạo settings/default metrics, load/reload/validate `chat.ini`.
- Thêm unit test cho logic thuần: queue cap, UTF conversion, history, clamp config.

### Phase 2 - Capture và fail-safe hooks

- Cài/enable hook `AddChatMessage` và `AddMessage`, luôn chain original.
- Tách hook registry để rollback các hook đã cài nếu một bước thất bại.
- Thêm readiness state và native fallback.
- Không dùng packet 228-232 cho standard chat; giữ chúng độc lập cho feature channel tương lai.

### Phase 3 - Renderer MVP

- Tạo resource/font/primitive và tích hợp `Present`, `Lost`, `Reset`, `Shutdown`.
- Render background, timestamp, prefix, text, composer và caret theo palette/metrics.
- Implement word wrap, clipping, visible range và bottom anchoring.
- Cache measurement/wrapped lines; invalidate khi message/font/width/scale đổi.
- Bảo toàn D3D state và phối hợp thứ tự render với nametag.

### Phase 4 - Input Unicode và IME

- Hook/restore WndProc idempotent và đúng HWND lifecycle.
- Implement caret theo UTF-16 code point boundary, selection, Home/End, Ctrl+Arrow, Backspace/Delete.
- Implement clipboard `CF_UNICODETEXT`, Ctrl+A/C/V/X và Shift+Insert.
- Implement `WM_IME_STARTCOMPOSITION`, `WM_IME_COMPOSITION`, `WM_IME_ENDCOMPOSITION` với preedit/commit.
- Thêm history Up/Down, max input length theo byte sau conversion và thông báo lỗi không phá buffer.

### Phase 5 - Send/open/close integration

- Đồng bộ `CInput::Open`/`Close` với custom composer và cursor/game input state.
- Enter convert/gửi đúng một lần qua original `CInput::Send`; ESC đóng không gửi.
- Ngăn double-send và re-entrancy.
- Ẩn native input theo cơ chế đã qua Phase 0; fallback original nếu custom state không `Ready`.

### Phase 6 - Scroll, fade và runtime settings

- Wheel scroll 3 dòng, drag scrollbar, auto-scroll và chỉ báo message mới khi đang xem history.
- Smooth scroll theo delta time, clamp độc lập FPS.
- Fade sau idle và restore alpha khi hover/input active.
- Runtime reload cho scale, dimensions, font size, opacity, timestamp và animation flags.

### Phase 7 - Hardening và hiệu năng

- Audit mọi Win32/D3D/MinHook return value và teardown order.
- Chặn exception qua hook boundary; không dùng exception làm control flow trong render.
- Thêm debug counters: CPU time, draw calls, visible lines, cache hit/miss và allocations/event.
- Test spam message, message dài, malformed UTF-8, alt-tab/reset, reconnect và DLL unload.
- Profile 1000 message; chỉ render visible lines và không rebuild layout khi không invalidated.

### Phase 8 - Hoàn thiện tài liệu và release

- Cập nhật project/filter, README, config mẫu và hướng dẫn rollback.
- Build sạch Debug/Release Win32.
- Chạy acceptance matrix trong SA-MP 0.3.DL R1 và lưu kết quả.
- Chỉ sau khi MVP ổn định mới lập plan riêng cho search/filter/channel/rich features.

---

## 6. Tiêu chí nghiệm thu MVP

### Functional

- Server chat, client/system message, prefix và màu hiển thị đúng, không trùng message.
- T/Enter/ESC, command `/...`, history, scroll và composer hoạt động như SA-MP quen thuộc.
- Gõ tiếng Việt bằng Microsoft Vietnamese/UniKey/EVKey không mất dấu hoặc double commit.
- Copy/paste Unicode, selection và caret không tách surrogate pair hoặc làm hỏng buffer.
- Native chat/input chỉ bị ẩn khi Custom Chat `Ready`; fallback xuất hiện khi cố tình làm hỏng config/resource/hook.

### Stability

- Alt+Tab, đổi resolution và device reset lặp lại không crash/leak/mất resource vĩnh viễn.
- Connect, disconnect, reconnect và unload giữ đúng hook/WndProc ownership.
- Không đọc `CChat::m_entry`, không đọc/ghi `CInput::m_szInput`, không patch byte trong `samp.dll`.
- Original SA-MP functions luôn được gọi ở các đường cần giữ logic gốc.

### Performance

- Render chỉ visible lines; không copy toàn bộ queue và không wrap toàn bộ history mỗi frame.
- Không tạo font/texture/resource hoặc đọc config trong mỗi `Present`.
- Mục tiêu profile: CPU chat dưới 0.2 ms/frame trung bình và giảm dưới 1 FPS tại 1000 message trên máy test đã thống nhất.
- Mọi con số hiệu năng phải lấy từ build Release và ghi kèm cấu hình máy/FPS baseline.

---

## 7. Rủi ro chính và giảm thiểu

| Rủi ro | Ảnh hưởng | Giảm thiểu |
|---|---|---|
| Offset/calling convention sai | Crash hoặc stack corruption | Validate module range, enable từng hook, fail closed về native |
| Xung đột native input | Double input/double send | Spike Phase 0, readiness gate, re-entrancy guard |
| WndProc bị mod khác thay thế | Mất input hoặc crash unload | Chain đúng proc trước, restore chỉ khi còn ownership |
| D3D state leak | Hỏng UI/game render | State block, test trước/sau render, lifecycle tập trung |
| IME gửi composition hai lần | Sai tiếng Việt | Tách preedit/commit, test nhiều bộ gõ |
| UTF-8 vượt giới hạn SA-MP | Cắt giữa code point | Validate byte length trước send, truncate theo boundary hoặc từ chối rõ ràng |
| Mutex/allocation trong frame | Stutter | Drain queue ngắn, visible cache, profile allocation |
| SRS quá rộng | Không thể nghiệm thu | Khóa MVP, chuyển feature nâng cao sang plan sau |

---

## 8. Các điểm cần duyệt

1. Duyệt phạm vi MVP và hoãn các tính năng trong mục "Không đưa vào MVP".
2. Duyệt ngoại lệ hook duy nhất `CChat::Render` để ẩn native chat, có readiness gate và fallback original.
3. Duyệt Phase 0 làm cổng kỹ thuật bắt buộc trước khi triển khai toàn bộ UI/input.
4. Duyệt dùng `chat.ini` thay cho JSON để không thêm dependency parser vào DLL x86.
5. Xác nhận target duy nhất của MVP là SA-MP 0.3.DL R1, `Release | Win32`, Visual Studio 2019/v142.

Sau khi năm điểm trên được duyệt, implementation mới bắt đầu từ Phase 0. Nếu điểm 2 không được duyệt, yêu cầu "ẩn hoàn toàn native chat" phải được nới lỏng hoặc chọn một cơ chế can thiệp nội bộ khác có rủi ro cao hơn.

---

# GTAHUB Custom Chat System
## Software Requirements Specification (SRS)

Version:
1.0.0

Target:
SA:MP 0.3.DL R1

Language:
C++17

Platform:
Windows x86

Compiler:
Visual Studio 2019

Rendering:
Direct3D9

Hook:
MinHook

Author:
GTAHUB Development Team

---

# 1. Project Goal

Thiết kế và xây dựng lại hoàn toàn hệ thống Chat của SA:MP.

Không sử dụng giao diện chat mặc định.

Không sửa trực tiếp bộ nhớ nội bộ của SA:MP.

Không chỉnh sửa struct CChat.

Không chỉnh sửa struct CInput.

Không patch byte trong samp.dll.

Toàn bộ hệ thống hoạt động thông qua Hook và Direct3D9 Rendering.

Mục tiêu cuối cùng:

• Giao diện hiện đại
• Đơn giản
• Thân thiện
• Giống GTA
• Hiệu năng cao
• Không crash
• Không memory leak
• Không memory corruption

---

# 2. Design Philosophy

Hệ thống phải ưu tiên:

Readability

Consistency

Performance

Maintainability

User Experience

Không cố gắng tạo giao diện quá màu mè.

Người chơi phải cảm thấy đây giống một phần của GTA SA.

Không phải một Overlay.

Không phải Discord.

Không phải ImGui Demo.

Không phải CEF.

Không phải Web UI.

---

# 3. User Experience Goals

Người chơi mới chỉ cần nhìn 5 giây.

Là biết:

• Chat nằm ở đâu

• Gõ ở đâu

• Scroll thế nào

• Tin nhắn mới xuất hiện ở đâu

Không cần hướng dẫn.

Không cần Tutorial.

Không cần Animation phức tạp.

---

# 4. Visual Style

Classic SA:MP

+

Modern Flat Design

+

Dark Theme

Không Cyberpunk.

Không RGB.

Không Neon.

Không Glass quá mạnh.

Không Blur quá nặng.

Cho phép:

✓ Shadow nhẹ

✓ Alpha

✓ Fade

✓ Rounded Corner

✓ Hover

✓ Smooth Scroll

✓ Border

Tất cả hiệu ứng phải phục vụ trải nghiệm.

Không được làm giảm FPS.

---

# 5. Color Palette

Window Background

#1E1E1E

Composer

#252526

Border

#404040

Text

#FFFFFF

Timestamp

#8E8E8E

Server

#FFD54F

System

#BBBBBB

Player

Theo màu server gửi.

Selection

#4A90E255

Placeholder

#888888

Error

#FF6666

Success

#66DD66

Hyperlink

#55AAFF

Mention

#00CCFF

Admin

#FF8800

Police

#4DA6FF

EMS

#FF66AA

Faction

Theo config.

---

# 6. Font

Primary

Tahoma

Fallback

Arial

Unicode:

Segoe UI Symbol

Size:

13

Line Height:

19

Padding:

6

Không dùng font lạ.

Không dùng font pixel.

Không dùng font quá mỏng.

---

# 7. Window Layout

Chat Window

Position:

Bottom Left

Margin Left

18 px

Margin Bottom

18 px

Width

520 px

Height

300 px

Padding

10 px

Border Radius

6 px

Opacity

92%

---

# 8. Composer

Height

38 px

Background

Dark

Border

1 px

Placeholder

"Nhập tin nhắn..."

Cursor

Blink

Selection

Windows Style

Clipboard

Supported

Ctrl+A

Ctrl+C

Ctrl+V

Ctrl+X

Shift+Arrow

Home

End

Delete

Backspace

History Up

History Down

---

# 9. Chat Line Layout

Một dòng gồm:

Timestamp

↓

Player Name

↓

Separator

↓

Message

Ví dụ

[18:52]

KhNguyen:

Xin chào

Timestamp màu xám.

Tên giữ nguyên màu server.

Tin nhắn màu trắng.

---

# 10. Timestamp

Format

HH:mm

Ví dụ

08:15

13:45

23:58

Có thể tắt trong Config.

---

# 11. Message Types

SERVER

SYSTEM

PLAYER

ADMIN

ERROR

WARNING

INFO

ACTION

ME

DO

RADIO

PHONE

FACTION

Mỗi loại có màu riêng.

Không hardcode.

Đọc từ Config.

---

# 12. Configuration

Toàn bộ giao diện phải cấu hình được.

Ví dụ:

chat.json

theme.json

font.json

Không hardcode màu.

Không hardcode kích thước.

Có thể Reload Config mà không cần Restart Game.

---

# 13. Performance Target

FPS Loss

< 1 FPS

CPU

< 0.2 ms/frame

Memory

< 10 MB

Không Allocate liên tục mỗi frame.

Không tạo std::string trong Render Loop nếu tránh được.

Cache mọi thứ có thể cache.

---

# 14. Coding Standard

Modern C++17

RAII

unique_ptr

deque

vector

unordered_map

constexpr

enum class

Không macro.

Không goto.

Không global variable tràn lan.

Không duplicated code.

Một class chỉ làm một nhiệm vụ.

SOLID nếu hợp lý.

---

# 15. Folder Structure

src/

Chat/

Renderer/

Input/

Hook/

Network/

Utils/

Logger/

Theme/

Fonts/

Resources/

Config/

ThirdParty/

vendor/

MinHook/

samp-api/

include/

docs/

assets/
# 16. Message Pipeline

Tuyệt đối KHÔNG đọc trực tiếp mảng:

CChat::m_entry[]

KHÔNG polling.

KHÔNG scan memory.

KHÔNG đọc history mỗi frame.

Pipeline chuẩn:

Server

↓

SA:MP

↓

CChat::AddChatMessage()

↓

Hook

↓

ChatManager::Push()

↓

Renderer

↓

Direct3D9

Đối với message nội bộ:

SA:MP

↓

CChat::AddMessage()

↓

Hook

↓

ChatManager

↓

Renderer

Đối với người chơi:

Keyboard

↓

InputManager

↓

Enter

↓

CInput::Send()

↓

Server

Không được tự tạo RakNet Packet.

Không được bypass Send().

==================================================

# 17. ChatManager

ChatManager là trung tâm của toàn bộ hệ thống.

Không render.

Không hook.

Không xử lý Direct3D.

Chỉ quản lý dữ liệu.

Class:

ChatManager

Chức năng:

Push()

Pop()

Clear()

GetMessages()

AddHistory()

ClearHistory()

FindMessage()

Filter()

Search()

GetVisibleMessages()

SetScroll()

GetScroll()

SaveHistory()

LoadHistory()

==================================================

# 18. ChatMessage Structure

class ChatMessage

Bao gồm:

uint64_t id

SYSTEMTIME timestamp

std::string prefix

std::string sender

std::string message

D3DCOLOR color

MessageType type

bool isLocal

bool isCommand

bool isSystem

bool isAdmin

bool isFaction

bool isRadio

bool isPhone

bool isAction

Không dùng char[].

Không dùng malloc.

==================================================

# 19. Message Queue

Container:

std::deque<ChatMessage>

Dung lượng:

200 message

Nếu vượt:

pop_front()

Không bao giờ để queue tăng vô hạn.

==================================================

# 20. Message History

History dùng riêng.

Không dùng chung queue.

Container:

std::deque<std::wstring>

Dung lượng:

50

History Up

↓

Message cũ

History Down

↓

Message mới

Không mất history khi đóng chat.

==================================================

# 21. InputManager

InputManager chịu trách nhiệm:

Unicode

Clipboard

Selection

Cursor

History

IME

Keyboard

Mouse

Không render.

Không hook D3D.

Không gửi packet.

==================================================

# 22. Text Buffer

Container:

std::wstring

Không dùng:

char*

Không dùng:

char[]

Không dùng:

strcpy()

Không dùng:

sprintf()

Mọi thao tác:

UTF-16

Khi gửi mới convert:

UTF-8

==================================================

# 23. Vietnamese Input

BẮT BUỘC.

Hỗ trợ:

Windows IME

Microsoft Vietnamese

UniKey

EVKey

OpenKey

Telex

VNI

Unicode dựng sẵn

Không bị:

Mất dấu

Double dấu

Sai ký tự

Cắt chuỗi

Lỗi MultiByteToWideChar

Lỗi WideCharToMultiByte

==================================================

# 24. Clipboard

Hỗ trợ:

Ctrl+C

Ctrl+V

Ctrl+X

Ctrl+A

Shift+Insert

Copy Unicode.

Paste Unicode.

==================================================

# 25. Keyboard

Enter

↓

Send

ESC

↓

Close Composer

Tab

↓

Auto Complete (tùy chọn)

Arrow Left

Arrow Right

Home

End

Delete

Backspace

Ctrl+Backspace

Ctrl+Delete

==================================================

# 26. Mouse

Mouse Wheel

↓

Scroll

Left Click

↓

Focus Composer

Drag ScrollBar

↓

Scroll

Double Click

↓

Select Word

Triple Click

↓

Select Line (tùy chọn)

==================================================

# 27. Scroll System

Smooth Scroll.

Không giật.

Không nhảy.

Wheel Speed:

3 dòng

Scrollbar Width:

6 px

Border Radius:

3 px

Auto Hide:

Có

Hover:

Hiện

==================================================

# 28. Fade

Không hoạt động:

10 giây

↓

Fade

Alpha:

255

↓

80

Animation:

200 ms

Hover

↓

255

==================================================

# 29. Notification

Tin nhắn mới:

Flash nhẹ.

Không nhấp nháy.

Không rung.

Không glow mạnh.

==================================================

# 30. Hyperlink

Nhận diện:

http://

https://

discord.gg/

Có màu xanh.

Click:

Copy Link

Không mở browser tự động.

==================================================

# 31. Mention

Nếu chứa:

Tên người chơi

↓

Highlight.

Có thể phát âm thanh nhẹ.

==================================================

# 32. Emoji

Không convert.

Hiển thị nếu font hỗ trợ.

Không crash nếu ký tự ngoài BMP.

==================================================

# 33. Selection

Selection giống Windows.

Background:

Blue Alpha

Text:

White

Unicode-safe.

==================================================

# 34. Cursor

Blink:

500ms

Width:

2px

Color:

White

==================================================

# 35. Placeholder

Hiển thị:

"Nhập tin nhắn..."

Color:

Gray

Ẩn khi có ký tự đầu tiên.

==================================================

# 36. Auto Complete (Optional)

Nếu nhập:

/

↓

Hiện danh sách command.

Ví dụ:

/help

/login

/register

/pm

/r

/f

Không ảnh hưởng hiệu năng.

==================================================

# 37. Filtering

Cho phép:

Ẩn:

System

Server

Faction

Phone

Radio

Admin

Action

Thông qua Config.

Không hardcode.

==================================================

# 38. Search

Ctrl+F

↓

Search Message

Highlight kết quả.

Không bắt buộc cho phiên bản đầu tiên.

==================================================

# 39. Export Log

Có thể Export:

TXT

UTF-8

Bao gồm:

Timestamp

Player

Message

==================================================

# 40. Future Features

Reaction

Reply

Copy Message

Favorite

Pin

Search

Channel

Tab Chat

Whisper Window

Được thiết kế mở để dễ mở rộng sau này.
---

# 15. Tùy chỉnh (Settings)

Hệ thống Custom Chat phải hỗ trợ đầy đủ các tùy chọn để người chơi có thể thay đổi theo sở thích.

Ví dụ:

- Scale UI (80% → 150%)
- Font Size
- Opacity
- Background Opacity
- Message Spacing
- Border Radius
- Timestamp On/Off
- Shadow On/Off
- Animation On/Off
- Smooth Scroll On/Off
- Auto Hide On/Off
- Hide Native Chat
- Show Join/Quit
- Show Server Messages
- Show Local Messages
- Chat Width
- Chat Height

Toàn bộ setting phải có thể thay đổi runtime.

Không cần restart game.

---

# 16. Performance

Đây là yêu cầu bắt buộc.

Không được render toàn bộ message mỗi frame.

Phải cache.

Ví dụ:

```
Visible Message

↓

Build Vertex

↓

Cache

↓

Render
```

Không render lại text nếu nội dung không đổi.

---

Không được:

- new
- delete
- malloc
- free

trong Present()

---

Không tạo:

std::string

std::wstring

vector

map

unordered_map

trong vòng lặp render.

---

Toàn bộ bộ nhớ phải được chuẩn bị từ trước.

---

FPS giảm tối đa:

1 FPS

khi có:

1000 message.

---

# 17. DirectX Resources

Font

Sprite

Texture

Vertex Buffer

Index Buffer

Line

Rectangle

phải được tạo một lần.

---

Khi Reset Device:

OnLostDevice()

↓

Release

↓

Reset

↓

Create lại

---

Không được memory leak.

---

# 18. Logging

Logger phải ghi đầy đủ:

```
CustomChat initialized

CustomChat destroyed

Font created

Font recreated

Input opened

Input closed

Clipboard paste

Clipboard copy

IME Start

IME End

Message Added

Message Removed

Scroll Changed

Opacity Changed

Animation Finished

Device Lost

Device Reset
```

Nếu xảy ra exception:

```
[ERROR]

Function

Reason

Stack (nếu có)

```

Không được crash game.

---

# 19. Coding Style

Code phải chia module rõ ràng.

Ví dụ:

```
CustomChat.h

CustomChat.cpp

CustomChatInput.cpp

CustomChatInput.h

CustomChatRender.cpp

CustomChatRender.h

CustomChatMessage.cpp

CustomChatMessage.h

CustomChatAnimation.cpp

CustomChatAnimation.h

CustomChatClipboard.cpp

CustomChatClipboard.h

CustomChatSettings.cpp

CustomChatSettings.h

CustomChatTheme.cpp

CustomChatTheme.h

CustomChatResources.cpp

CustomChatResources.h
```

Không được viết file 3000+ dòng.

Mỗi file chỉ đảm nhiệm một chức năng.

---

# 20. Tương thích

Bắt buộc tương thích:

- SA-MP 0.3.DL R1
- Win32 (x86)
- DirectX9
- Visual Studio 2019
- C++17
- MinHook
- sampapi 0.3.DL-1

Không phụ thuộc:

- ImGui
- CEF
- Chromium
- Qt
- SDL
- SFML
- Dear ImGui

---

# 21. Những điều KHÔNG ĐƯỢC LÀM

Không được sửa trực tiếp dữ liệu nội bộ của CChat nếu không thật sự cần thiết.

Không được ghi đè:

- m_nMode
- m_entry
- m_nPageSize
- m_nScrollPosition

Không được đọc trực tiếp:

```
CInput::m_szInput
```

Không được Patch Memory bừa bãi.

Không Hook hàm không cần thiết.

Không sử dụng polling liên tục.

Không sử dụng Sleep trong Render.

Không sử dụng Busy Wait.

Không tạo Thread mới chỉ để render.

Không làm giảm FPS.

Không làm tăng CPU Usage.

Không gây Memory Leak.

Không gây Access Violation.

Không gây C++ Exception.

Không làm crash SA-MP.

---

# 22. Kiến trúc mong muốn

```
Game

↓

SA-MP

↓

HookManager

├── Input Hook
├── D3D Present Hook
├── WndProc Hook

↓

Custom Chat Core

├── Input Manager
├── Clipboard Manager
├── IME Manager
├── Message Manager
├── Scroll Manager
├── Animation Manager
├── Theme Manager
├── Renderer
├── Resource Manager

↓

DirectX9
```

Toàn bộ module phải tách biệt rõ ràng.

Không phụ thuộc vòng tròn lẫn nhau.

---

# 23. Mục tiêu cuối cùng

Mục tiêu là tạo ra một hệ thống Chat hoàn toàn mới cho SA-MP với các tiêu chí:

- Giao diện đẹp, tối giản, hiện đại nhưng vẫn giữ phong cách cổ điển của SA-MP.
- Dễ sử dụng đối với cả người chơi mới và người chơi lâu năm.
- Hỗ trợ đầy đủ tiếng Việt (Unicode, IME, Telex, VNI, clipboard).
- Hiệu năng cực cao, tối ưu cho máy cấu hình thấp.
- Không làm ảnh hưởng đến gameplay hoặc FPS.
- Dễ bảo trì, dễ mở rộng trong tương lai.
- Mã nguồn sạch, chia module rõ ràng, tuân thủ C++17.
- Không sử dụng framework UI bên ngoài.
- Tương thích hoàn toàn với SA-MP 0.3.DL R1 và sampapi 0.3.DL-1.
- Thay thế hoàn toàn giao diện chat mặc định nhưng vẫn giữ nguyên trải nghiệm sử dụng quen thuộc.
- Ưu tiên sự ổn định tuyệt đối: không crash, không memory corruption, không exception, không leak.

Đây là yêu cầu bắt buộc. Hệ thống chỉ được xem là hoàn thành khi đáp ứng đầy đủ tất cả các tiêu chí trên.
```
---

# 15. Tùy chỉnh (Settings)

Hệ thống Custom Chat phải hỗ trợ đầy đủ các tùy chọn để người chơi có thể thay đổi theo sở thích.

Ví dụ:

- Scale UI (80% → 150%)
- Font Size
- Opacity
- Background Opacity
- Message Spacing
- Border Radius
- Timestamp On/Off
- Shadow On/Off
- Animation On/Off
- Smooth Scroll On/Off
- Auto Hide On/Off
- Hide Native Chat
- Show Join/Quit
- Show Server Messages
- Show Local Messages
- Chat Width
- Chat Height

Toàn bộ setting phải có thể thay đổi runtime.

Không cần restart game.

---

# 16. Performance

Đây là yêu cầu bắt buộc.

Không được render toàn bộ message mỗi frame.

Phải cache.

Ví dụ:

```
Visible Message

↓

Build Vertex

↓

Cache

↓

Render
```

Không render lại text nếu nội dung không đổi.

---

Không được:

- new
- delete
- malloc
- free

trong Present()

---

Không tạo:

std::string

std::wstring

vector

map

unordered_map

trong vòng lặp render.

---

Toàn bộ bộ nhớ phải được chuẩn bị từ trước.

---

FPS giảm tối đa:

1 FPS

khi có:

1000 message.

---

# 17. DirectX Resources

Font

Sprite

Texture

Vertex Buffer

Index Buffer

Line

Rectangle

phải được tạo một lần.

---

Khi Reset Device:

OnLostDevice()

↓

Release

↓

Reset

↓

Create lại

---

Không được memory leak.

---

# 18. Logging

Logger phải ghi đầy đủ:

```
CustomChat initialized

CustomChat destroyed

Font created

Font recreated

Input opened

Input closed

Clipboard paste

Clipboard copy

IME Start

IME End

Message Added

Message Removed

Scroll Changed

Opacity Changed

Animation Finished

Device Lost

Device Reset
```

Nếu xảy ra exception:

```
[ERROR]

Function

Reason

Stack (nếu có)

```

Không được crash game.

---

# 19. Coding Style

Code phải chia module rõ ràng.

Ví dụ:

```
CustomChat.h

CustomChat.cpp

CustomChatInput.cpp

CustomChatInput.h

CustomChatRender.cpp

CustomChatRender.h

CustomChatMessage.cpp

CustomChatMessage.h

CustomChatAnimation.cpp

CustomChatAnimation.h

CustomChatClipboard.cpp

CustomChatClipboard.h

CustomChatSettings.cpp

CustomChatSettings.h

CustomChatTheme.cpp

CustomChatTheme.h

CustomChatResources.cpp

CustomChatResources.h
```

Không được viết file 3000+ dòng.

Mỗi file chỉ đảm nhiệm một chức năng.

---

# 20. Tương thích

Bắt buộc tương thích:

- SA-MP 0.3.DL R1
- Win32 (x86)
- DirectX9
- Visual Studio 2019
- C++17
- MinHook
- sampapi 0.3.DL-1

Không phụ thuộc:

- ImGui
- CEF
- Chromium
- Qt
- SDL
- SFML
- Dear ImGui

---

# 21. Những điều KHÔNG ĐƯỢC LÀM

Không được sửa trực tiếp dữ liệu nội bộ của CChat nếu không thật sự cần thiết.

Không được ghi đè:

- m_nMode
- m_entry
- m_nPageSize
- m_nScrollPosition

Không được đọc trực tiếp:

```
CInput::m_szInput
```

Không được Patch Memory bừa bãi.

Không Hook hàm không cần thiết.

Không sử dụng polling liên tục.

Không sử dụng Sleep trong Render.

Không sử dụng Busy Wait.

Không tạo Thread mới chỉ để render.

Không làm giảm FPS.

Không làm tăng CPU Usage.

Không gây Memory Leak.

Không gây Access Violation.

Không gây C++ Exception.

Không làm crash SA-MP.

---

# 22. Kiến trúc mong muốn

```
Game

↓

SA-MP

↓

HookManager

├── Input Hook
├── D3D Present Hook
├── WndProc Hook

↓

Custom Chat Core

├── Input Manager
├── Clipboard Manager
├── IME Manager
├── Message Manager
├── Scroll Manager
├── Animation Manager
├── Theme Manager
├── Renderer
├── Resource Manager

↓

DirectX9
```

Toàn bộ module phải tách biệt rõ ràng.

Không phụ thuộc vòng tròn lẫn nhau.

---

# 23. Mục tiêu cuối cùng

Mục tiêu là tạo ra một hệ thống Chat hoàn toàn mới cho SA-MP với các tiêu chí:

- Giao diện đẹp, tối giản, hiện đại nhưng vẫn giữ phong cách cổ điển của SA-MP.
- Dễ sử dụng đối với cả người chơi mới và người chơi lâu năm.
- Hỗ trợ đầy đủ tiếng Việt (Unicode, IME, Telex, VNI, clipboard).
- Hiệu năng cực cao, tối ưu cho máy cấu hình thấp.
- Không làm ảnh hưởng đến gameplay hoặc FPS.
- Dễ bảo trì, dễ mở rộng trong tương lai.
- Mã nguồn sạch, chia module rõ ràng, tuân thủ C++17.
- Không sử dụng framework UI bên ngoài.
- Tương thích hoàn toàn với SA-MP 0.3.DL R1 và sampapi 0.3.DL-1.
- Thay thế hoàn toàn giao diện chat mặc định nhưng vẫn giữ nguyên trải nghiệm sử dụng quen thuộc.
- Ưu tiên sự ổn định tuyệt đối: không crash, không memory corruption, không exception, không leak.

Đây là yêu cầu bắt buộc. Hệ thống chỉ được xem là hoàn thành khi đáp ứng đầy đủ tất cả các tiêu chí trên.
```
---

# 39. Hooking Strategy

Toàn bộ hệ thống phải sử dụng chiến lược Hook tối thiểu (Minimal Hooking).

Chỉ Hook những gì thực sự cần thiết.

Các Hook bắt buộc:

- Direct3D9 Present
- Direct3D9 Reset
- WndProc
- CInput::Open
- CInput::Close
- CInput::Send

Không Hook thêm nếu không có lý do chính đáng.

---

## Không Hook

Không Hook:

- CChat::Render
- CChat::RenderEntry
- CChat::Draw
- CChat::PageUp
- CChat::PageDown

vì Custom Chat tự render hoàn toàn.

---

## Native Chat

Native Chat vẫn tồn tại.

Nhưng không được render.

Không sửa dữ liệu.

Không Patch.

Không ghi đè struct.

Không thay đổi layout.

---

# 40. Native Chat Compatibility

Custom Chat phải hoạt động song song với Chat gốc.

Ví dụ:

SA-MP

↓

Server gửi Chat

↓

Native CChat nhận

↓

CustomChat nhận

↓

Native Chat không render

↓

Custom Chat render

Toàn bộ logic của SA-MP vẫn hoạt động.

Không làm hỏng gameplay.

---

# 41. Message Capture

Message không được lấy bằng cách đọc trực tiếp:

```
chat->m_entry
```

Không scan bộ nhớ.

Không reverse linked list.

Không reverse buffer.

Không polling.

---

Message chỉ được lấy thông qua:

Hook

hoặc

RakNet Packet

hoặc

Callback nội bộ.

---

Nếu Hook thất bại.

Game vẫn phải chạy bình thường.

---

# 42. Input System

Input hoàn toàn độc lập.

Không sử dụng:

```
CInput::m_szInput
```

Không dùng EditBox của SA-MP.

Không dùng DXUT Edit.

Toàn bộ nhập liệu dùng:

WndProc

↓

UTF16 Buffer

↓

Render

↓

Convert UTF8

↓

Send()

---

# 43. WndProc

Hook WndProc bằng:

```
SetWindowLongPtr()

GWLP_WNDPROC
```

Không Poll Keyboard.

Không GetAsyncKeyState cho text.

Keyboard event:

WM_CHAR

WM_KEYDOWN

WM_KEYUP

WM_SYSKEYDOWN

WM_SYSKEYUP

WM_IME_*

WM_MOUSE*

---

Sau khi xử lý.

Phải gọi:

```
CallWindowProc(...)
```

để không làm hỏng game.

---

# 44. Caret

Con trỏ nhập liệu:

Màu trắng.

Độ rộng:

2 pixel.

Blink:

500 ms.

Không dùng caret của Windows.

Tự render.

---

# 45. Text Editing

Hỗ trợ đầy đủ:

Insert

Overwrite (tuỳ chọn)

Backspace

Delete

Ctrl+Backspace

Ctrl+Delete

Home

End

Ctrl+Left

Ctrl+Right

Selection

Copy

Paste

Undo

Redo

---

# 46. Clipboard

Clipboard Unicode.

Dùng:

```
OpenClipboard()

GetClipboardData()

SetClipboardData()
```

Định dạng:

```
CF_UNICODETEXT
```

Không dùng ANSI.

---

# 47. UTF-8 Pipeline

Pipeline chuẩn:

```
Windows

↓

UTF16

↓

Buffer

↓

Render

↓

UTF8

↓

SA-MP Send()

```

Không convert nhiều lần.

Không convert mỗi frame.

---

# 48. Resource Lifetime

Tất cả resource phải có lifecycle rõ ràng.

Ví dụ:

```
Initialize()

↓

Create()

↓

Render()

↓

Lost()

↓

Reset()

↓

Destroy()

```

Không được tạo Font trong Render.

Không được Release trong Present.

---

# 49. Renderer

Renderer chỉ có nhiệm vụ:

Draw.

Không xử lý:

Logic.

Input.

Clipboard.

Animation.

Network.

---

Renderer phải Stateless.

---

# 50. Message Manager

MessageManager chịu trách nhiệm:

Push()

Pop()

Clear()

Filter()

History()

Search()

Scroll()

Không render.

Không Input.

Không D3D.

---

# 51. Input Manager

InputManager chịu trách nhiệm:

Keyboard

IME

Clipboard

Caret

Selection

Undo

Redo

History

Autocomplete (dự phòng)

Không render.

---

# 52. Animation Manager

Animation độc lập.

Không nằm trong Renderer.

Mỗi animation:

Start

Update

Finish

Cancel

---

Không dùng Sleep.

Không Timer riêng.

Update theo Delta Time.

---

# 53. Theme Manager

Theme chỉ quản lý:

Color

Radius

Shadow

Opacity

Padding

Margin

Font

Spacing

Không render.

---

# 54. Debug Mode

Có chế độ:

```
CHAT_DEBUG
```

Hiển thị:

FPS

Message Count

Visible Count

Input Length

Scroll Offset

Cache Count

Draw Calls

Vertex Count

CPU Time

GPU Time (nếu đo được)

---

Chỉ bật trong Debug Build.

Release Build phải tắt hoàn toàn.

---

# 55. Final Goal

Sản phẩm cuối cùng phải có cảm giác như:

"Nếu SA-MP được phát hành năm 2025 thì giao diện chat mặc định sẽ như thế này."

Người chơi cũ mở game sẽ thấy:

- Quen thuộc.
- Không bị choáng ngợp.
- Dễ dùng.
- Gọn gàng.
- Hiện đại.
- Mượt.
- Chuyên nghiệp.

Người chơi mới sẽ thấy:

- Dễ đọc.
- Dễ nhập.
- Hỗ trợ tiếng Việt hoàn chỉnh.
- Không cần cài thêm.
- Không gặp lỗi font.
- Không gặp lỗi IME.

Đây là tiêu chuẩn cuối cùng của toàn bộ dự án.
````md
---

# 56. User Experience (UX)

Đây là yêu cầu quan trọng không kém hiệu năng.

Người chơi phải cảm thấy:

- Dễ nhìn.
- Dễ đọc.
- Dễ nhập.
- Dễ sử dụng.
- Không khác biệt quá nhiều so với chat mặc định.
- Không cần học lại cách sử dụng.

Toàn bộ thao tác phải giống SA-MP gốc.

Ví dụ:

```
T

↓

Mở chat

↓

Nhập

↓

Enter

↓

Gửi

↓

ESC

↓

Đóng
```

Không thay đổi thói quen người chơi.

---

# 57. Accessibility

Hỗ trợ người chơi:

- Màn hình nhỏ.
- Laptop.
- Máy cấu hình yếu.
- Độ phân giải thấp.
- DPI cao.
- Người thị lực kém.

Cho phép:

- Scale UI
- Scale Font
- Opacity
- Contrast

---

# 58. Notification

Không sử dụng popup.

Không sử dụng toast.

Không hiệu ứng lòe loẹt.

Khi có message mới:

Chỉ:

- Fade
- Slide nhẹ

Nếu chat đang đóng:

Hiển thị bình thường.

Không rung.

Không nhấp nháy.

---

# 59. Sound

Không phát âm thanh.

Không beep.

Không click.

Nếu sau này muốn thêm.

Phải có module riêng.

Mặc định:

OFF.

---

# 60. Future Extension

Kiến trúc phải đủ sạch để sau này thêm:

- Emoji Picker
- Mention
- Reply
- Copy Message
- Search Message
- Filter
- Tab Chat
- Channel Chat
- Friend Chat
- Voice Icon
- Rich Text

mà không cần sửa toàn bộ hệ thống.

---

# 61. Networking

Custom Chat không được thay đổi giao thức mạng.

Không sửa RakNet.

Không sửa Packet.

Không thay đổi thứ tự gửi.

Không delay.

Không queue thêm.

Chỉ:

Hiển thị.

Nhập.

Gửi.

---

# 62. Security

Không được:

- Execute Script.
- HTML.
- BBCode.
- Markdown.
- RichText.
- Lua.
- JavaScript.

Message luôn được render dưới dạng Plain Text.

Không thực thi bất kỳ nội dung nào.

---

# 63. Error Recovery

Nếu Font lỗi.

↓

Tự tạo lại.

Nếu Sprite lỗi.

↓

Tự tạo lại.

Nếu Device Lost.

↓

Khôi phục.

Nếu IME lỗi.

↓

Disable IME.

↓

Tiếp tục hoạt động.

Nếu Clipboard lỗi.

↓

Bỏ qua.

↓

Không crash.

---

# 64. Memory Management

Không dùng:

```
new

delete
```

trực tiếp.

Ưu tiên:

```
std::unique_ptr

std::vector

std::deque

std::array
```

Không rò rỉ bộ nhớ.

Kiểm tra bằng:

Visual Studio Diagnostic

CRT Leak Detector

---

# 65. Render Pipeline

Pipeline mong muốn:

```
BeginScene

↓

Update Animation

↓

Update Layout

↓

Build Visible Messages

↓

Draw Background

↓

Draw Messages

↓

Draw Scrollbar

↓

Draw Composer

↓

Draw Caret

↓

EndScene
```

Không render dư thừa.

Không DrawText nhiều lần cho cùng một nội dung.

---

# 66. Message Layout Cache

Message sau khi xuống dòng.

↓

Cache.

Không wrap lại mỗi frame.

Chỉ wrap khi:

- Scale thay đổi.
- Font thay đổi.
- Width thay đổi.
- Message mới.

---

# 67. Input History

Lưu:

20~50 dòng gần nhất.

Nhấn:

↑

↓

để duyệt.

Giống terminal.

Giống SA-MP.

Không mất khi đóng chat.

Chỉ reset khi thoát game.

---

# 68. Command Support

Nếu nhập:

```
/help

/login

/register

/admin
```

Vẫn gửi nguyên văn.

Không xử lý nội bộ.

Không parse.

Không sửa.

Toàn bộ command gửi cho server.

---

# 69. Placeholder

Khi chưa nhập.

Hiển thị:

```
Nhập tin nhắn...

```

hoặc

```
Type a message...

```

Màu xám.

Không gây nhầm lẫn với text thật.

---

# 70. Caret Position

Caret phải tính theo:

Unicode.

Không theo byte.

Ví dụ:

```
đ

ă

ơ

ứ

😊
```

Mỗi ký tự là 1 vị trí.

Không bị lệch.

---

# 71. Final Acceptance Checklist

Dự án chỉ được coi là hoàn thành khi đáp ứng tất cả điều kiện sau:

✅ Không crash sau nhiều giờ chơi.

✅ Không memory leak.

✅ Không access violation.

✅ Không C++ exception.

✅ Không SEH exception.

✅ Không mất FPS đáng kể.

✅ Gõ tiếng Việt đầy đủ (Telex, VNI, IME).

✅ Unicode hoạt động hoàn chỉnh.

✅ Copy/Paste Unicode.

✅ Scroll mượt.

✅ Word Wrap chính xác.

✅ Tự động xuống dòng.

✅ Auto Hide hoạt động.

✅ Native Chat được ẩn hoàn toàn nhưng logic SA-MP vẫn giữ nguyên.

✅ Không ghi đè bộ nhớ nội bộ nguy hiểm.

✅ Không chỉnh sửa struct CChat/CInput trái phép.

✅ Build sạch.

✅ Code production-ready.

---

# Yêu cầu cuối cùng dành cho AI

Hãy đóng vai là một **Senior C++ Engine Developer** có kinh nghiệm phát triển client game DirectX9, reverse engineering SA-MP và xây dựng UI hiệu năng cao.

Mọi quyết định thiết kế phải ưu tiên:

1. Ổn định.
2. Khả năng bảo trì.
3. Hiệu năng.
4. Trải nghiệm người dùng.
5. Khả năng mở rộng lâu dài.

Nếu phát hiện bất kỳ yêu cầu nào có nguy cơ gây crash, memory corruption hoặc làm giảm tính ổn định của SA-MP, hãy chủ động đề xuất phương án thay thế an toàn hơn thay vì triển khai một cách máy móc.

Toàn bộ mã nguồn phải đạt chất lượng production và có thể sử dụng trực tiếp trong một launcher/client SA-MP thương mại.
````
---

# 72. Code Architecture Standards

Toàn bộ dự án phải được tổ chức giống một dự án Engine thực thụ.

Không viết kiểu "mod script".

Không viết kiểu "sample code".

Không viết kiểu "proof of concept".

Mọi module phải có trách nhiệm riêng.

Không module nào được làm quá nhiều việc.

Ví dụ:

```
CustomChat/

├── Core/
├── Render/
├── Input/
├── Clipboard/
├── IME/
├── Theme/
├── Animation/
├── Layout/
├── Cache/
├── Resources/
├── Settings/
├── Utils/
├── Debug/
```

---

# 73. Namespace

Toàn bộ code phải nằm trong namespace riêng.

Ví dụ:

```cpp
namespace HUB
{

namespace Chat
{

}

}
```

Không được pollute Global Namespace.

Không dùng:

```
using namespace std;
```

---

# 74. Const Correctness

Các hàm không thay đổi dữ liệu phải khai báo:

```
const
```

Ví dụ:

```cpp
const ChatMessage&

const std::wstring&

const Theme&
```

Không copy dữ liệu không cần thiết.

---

# 75. Thread Safety

Mặc dù SA-MP chủ yếu chạy một Game Thread.

Toàn bộ kiến trúc vẫn phải chuẩn bị cho khả năng đa luồng trong tương lai.

Ví dụ:

- Message Queue
- Logger
- Clipboard
- Settings

phải có khả năng thread-safe.

Không sử dụng global mutable data tràn lan.

---

# 76. Logging Levels

Logger chia thành:

```
TRACE

DEBUG

INFO

WARNING

ERROR

FATAL
```

Ví dụ:

```
[INFO]

CustomChat Initialized

[WARNING]

Font recreation

[ERROR]

CreateTexture failed

[FATAL]

Device Lost
```

Release Build:

Tắt TRACE.

Tắt DEBUG.

---

# 77. Build Configurations

Phải hỗ trợ:

```
Debug

Release
```

Debug:

- Log đầy đủ
- Assert
- Diagnostic

Release:

- Tối ưu
- Không log rác
- Không assert

---

# 78. Assertions

Debug Build:

Sử dụng:

```
assert()

_HUB_ASSERT()

```

Release Build:

Không được crash.

Chỉ ghi log.

---

# 79. Resource Ownership

Mỗi Resource chỉ có một Owner.

Ví dụ:

Font

↓

ResourceManager

Sprite

↓

ResourceManager

Texture

↓

ResourceManager

Không nhiều module cùng Release một object.

---

# 80. Device Lost Strategy

Khi:

Alt+Tab

↓

Device Lost

↓

Release Resource

↓

Reset Device

↓

Recreate Resource

↓

Continue

Người chơi không phải reconnect.

Không crash.

---

# 81. FPS Stability

Ở:

60 FPS

↓

144 FPS

↓

240 FPS

↓

360 FPS

Animation phải giống nhau.

Không phụ thuộc FPS.

Sử dụng:

```
Delta Time
```

Không dùng:

```
Sleep()

```

---

# 82. Layout Engine

Layout phải tự tính toán.

Không hardcode pixel.

Ví dụ:

```
Window Width

↓

Padding

↓

Content Width

↓

Scrollbar

↓

Input

↓

Final Layout
```

Không dùng:

```
x = 341

y = 212
```

ở mọi nơi.

---

# 83. UI Metrics

Toàn bộ kích thước phải định nghĩa tập trung.

Ví dụ:

```cpp
struct Metrics
{
    float padding;

    float spacing;

    float radius;

    float border;

    float shadow;

    float scrollbar;

    float inputHeight;
};
```

Không magic number.

---

# 84. Localization Ready

Mặc dù hiện tại chỉ cần:

Tiếng Việt

Tiếng Anh

Nhưng kiến trúc phải hỗ trợ thêm:

- Thai
- Russian
- Chinese
- Japanese

sau này.

Không hardcode text.

Ví dụ:

```
"Nhập tin nhắn..."

↓

Language Table
```

---

# 85. Configuration File

Toàn bộ Setting lưu trong:

```
JSON

hoặc

INI
```

Ví dụ:

```
chat.json

settings.json

config.ini
```

Tự load khi khởi động.

Tự save khi thay đổi.

Không cần restart.

---

# 86. Error Handling Policy

Mọi API của Windows.

DirectX.

MinHook.

IME.

Clipboard.

File.

Network.

đều phải kiểm tra kết quả trả về.

Ví dụ:

```cpp
HRESULT hr = ...

if (FAILED(hr))
{
    Log...

    return;
}
```

Không bỏ qua lỗi.

---

# 87. Documentation

Mỗi module phải có comment đầu file.

Ví dụ:

```
Purpose

Responsibilities

Dependencies

Thread Safety

Lifecycle
```

Không cần comment từng dòng.

Chỉ comment nơi khó hiểu.

---

# 88. Final Quality Goal

Đây không chỉ là một Custom Chat.

Đây là một Framework Chat hoàn chỉnh.

Có thể tái sử dụng cho:

- SA-MP
- open.mp
- Multiplayer Client
- Launcher
- Client Mod

với thay đổi tối thiểu.

Kiến trúc phải đủ tốt để sử dụng trong nhiều năm mà không cần viết lại.

---
