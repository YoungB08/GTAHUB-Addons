# OMPVoiceCore installation

## Server

1. Copy `plugins/ompvoice.dll` into the open.mp `components/` directory.
2. Keep the component in the server process; it binds voice UDP port `7775`
   independently of the game server port.
3. Copy `ompvoice.inc` into `pawno/include` and include it from the gamemode.
4. Use the `OV_*` natives after the Pawn component has loaded.

## Client

1. Copy `ov_client.asi` beside the SA:MP 0.3.DL R1 executable.
2. Copy the complete `ompvoice/` directory beside the ASI. Keep its packaged
   32-bit `bass.dll` and `bass_fx.dll` inside that directory so existing game
   DLLs with the same names do not need to be replaced. CMake
   downloads these files from the official checksum-pinned archives; review
   the BASS license before redistributing a commercial build.
3. The client creates
   `logs`, `records`, and `debug` directories automatically.
4. Set the server address in the client configuration when it is not local.

The game client and open.mp server must be allowed to send UDP traffic on port
7775. `/ovdiag` writes a diagnostic report under `ompvoice/debug`.

ASI loaders that unload modules dynamically must call the exported
`OV_Shutdown(10000)` function and require a nonzero result before calling
`FreeLibrary`. Process termination uses the nonblocking `DllMain` fallback.
