# Third-party dependencies

Open-source dependencies are pinned and fetched by CMake. BASS and BASS FX are
proprietary runtime dependencies and are loaded dynamically by the client. Put
the official 32-bit `bass.dll` and `bass_fx.dll` beside `ov_client.asi`; no
unverified binary is committed to this repository.
