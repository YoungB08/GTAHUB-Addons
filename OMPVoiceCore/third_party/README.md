# Third-party dependencies

Open-source dependencies are pinned and fetched by CMake. BASS and BASS FX are
proprietary runtime dependencies loaded dynamically by the client. For Win32,
CMake fetches the official archives by SHA-256 and copies their DLLs beside
`ov_client.asi`; no unverified binary is committed to this repository. The
generated package includes the upstream text files under `licenses/`.
