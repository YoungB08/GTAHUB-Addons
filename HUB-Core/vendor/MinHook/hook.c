#include <windows.h>
#include <string.h>
#include "MinHook.h"
#include "buffer.h"
#include "trampoline.h"

typedef struct _HOOK_ENTRY {
    void*       pTarget;
    void*       pDetour;
    TRAMPOLINE  ct;
    BOOL        isEnabled;
    BOOL        isQueued;
} HOOK_ENTRY, *PHOOK_ENTRY;

#define MAX_HOOKS 64
static HOOK_ENTRY g_Hooks[MAX_HOOKS];
static UINT       g_HookCount = 0;
static BOOL       g_Initialized = FALSE;

const char* WINAPI MH_StatusToString(MH_STATUS status) {
    switch (status) {
        case MH_OK: return "MH_OK";
        case MH_ERROR_ALREADY_INITIALIZED: return "MH_ERROR_ALREADY_INITIALIZED";
        case MH_ERROR_NOT_INITIALIZED: return "MH_ERROR_NOT_INITIALIZED";
        case MH_ERROR_ALREADY_CREATED: return "MH_ERROR_ALREADY_CREATED";
        case MH_ERROR_NOT_CREATED: return "MH_ERROR_NOT_CREATED";
        case MH_ERROR_ENABLED: return "MH_ERROR_ENABLED";
        case MH_ERROR_DISABLED: return "MH_ERROR_DISABLED";
        case MH_ERROR_NOT_EXECUTABLE: return "MH_ERROR_NOT_EXECUTABLE";
        case MH_ERROR_UNSUPPORTED_FUNCTION: return "MH_ERROR_UNSUPPORTED_FUNCTION";
        case MH_ERROR_MEMORY_ALLOC: return "MH_ERROR_MEMORY_ALLOC";
        case MH_ERROR_MEMORY_PROTECT: return "MH_ERROR_MEMORY_PROTECT";
        default: return "MH_UNKNOWN";
    }
}

MH_STATUS WINAPI MH_Initialize(VOID) {
    if (g_Initialized) return MH_ERROR_ALREADY_INITIALIZED;
    InitializeBuffer();
    memset(g_Hooks, 0, sizeof(g_Hooks));
    g_HookCount = 0;
    g_Initialized = TRUE;
    return MH_OK;
}

MH_STATUS WINAPI MH_Uninitialize(VOID) {
    if (!g_Initialized) return MH_ERROR_NOT_INITIALIZED;
    for (UINT i = 0; i < g_HookCount; ++i) {
        if (g_Hooks[i].isEnabled) {
            MH_DisableHook(g_Hooks[i].pTarget);
        }
    }
    UninitializeBuffer();
    g_Initialized = FALSE;
    return MH_OK;
}

MH_STATUS WINAPI MH_CreateHook(LPVOID pTarget, LPVOID pDetour, LPVOID *ppOriginal) {
    if (!g_Initialized) return MH_ERROR_NOT_INITIALIZED;
    if (pTarget == NULL || pDetour == NULL) return MH_ERROR_NOT_EXECUTABLE;

    for (UINT i = 0; i < g_HookCount; ++i) {
        if (g_Hooks[i].pTarget == pTarget) return MH_ERROR_ALREADY_CREATED;
    }

    if (g_HookCount >= MAX_HOOKS) return MH_ERROR_MEMORY_ALLOC;

    PHOOK_ENTRY pHook = &g_Hooks[g_HookCount];
    pHook->pTarget = pTarget;
    pHook->pDetour = pDetour;
    pHook->ct.pTarget = pTarget;

    if (!CreateTrampoline32(&pHook->ct)) {
        return MH_ERROR_UNSUPPORTED_FUNCTION;
    }

    if (ppOriginal != NULL) {
        *ppOriginal = pHook->ct.pTrampoline;
    }

    g_HookCount++;
    return MH_OK;
}

MH_STATUS WINAPI MH_EnableHook(LPVOID pTarget) {
    if (!g_Initialized) return MH_ERROR_NOT_INITIALIZED;

    for (UINT i = 0; i < g_HookCount; ++i) {
        PHOOK_ENTRY pHook = &g_Hooks[i];
        if (pTarget == MH_ALL_HOOKS || pHook->pTarget == pTarget) {
            if (!pHook->isEnabled) {
                DWORD oldProtect = 0;
                if (!VirtualProtect(pHook->pTarget, pHook->ct.patchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                    return MH_ERROR_MEMORY_PROTECT;
                }

                BYTE patch[8];
                patch[0] = 0xE9; // JMP rel32
                *(DWORD*)(patch + 1) = (DWORD)pHook->pDetour - ((DWORD)pHook->pTarget + 5);

                for (UINT b = 5; b < pHook->ct.patchSize; ++b) {
                    patch[b] = 0x90; // NOP padding
                }

                memcpy(pHook->pTarget, patch, pHook->ct.patchSize);
                FlushInstructionCache(GetCurrentProcess(), pHook->pTarget, pHook->ct.patchSize);

                DWORD unusedProtect = 0;
                VirtualProtect(pHook->pTarget, pHook->ct.patchSize, oldProtect, &unusedProtect);

                pHook->isEnabled = TRUE;
            }
            if (pTarget != MH_ALL_HOOKS) return MH_OK;
        }
    }
    return MH_OK;
}

MH_STATUS WINAPI MH_DisableHook(LPVOID pTarget) {
    if (!g_Initialized) return MH_ERROR_NOT_INITIALIZED;

    for (UINT i = 0; i < g_HookCount; ++i) {
        PHOOK_ENTRY pHook = &g_Hooks[i];
        if (pTarget == MH_ALL_HOOKS || pHook->pTarget == pTarget) {
            if (pHook->isEnabled) {
                DWORD oldProtect = 0;
                if (!VirtualProtect(pHook->pTarget, pHook->ct.patchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                    return MH_ERROR_MEMORY_PROTECT;
                }

                memcpy(pHook->pTarget, pHook->ct.backup, pHook->ct.patchSize);
                FlushInstructionCache(GetCurrentProcess(), pHook->pTarget, pHook->ct.patchSize);

                DWORD unusedProtect = 0;
                VirtualProtect(pHook->pTarget, pHook->ct.patchSize, oldProtect, &unusedProtect);

                pHook->isEnabled = FALSE;
            }
            if (pTarget != MH_ALL_HOOKS) return MH_OK;
        }
    }
    return MH_OK;
}

MH_STATUS WINAPI MH_RemoveHook(LPVOID pTarget) {
    if (!g_Initialized) return MH_ERROR_NOT_INITIALIZED;
    for (UINT i = 0; i < g_HookCount; ++i) {
        if (g_Hooks[i].pTarget == pTarget) {
            if (g_Hooks[i].isEnabled) {
                MH_DisableHook(pTarget);
            }
            FreeBuffer(g_Hooks[i].ct.pTrampoline);
            for (UINT j = i; j < g_HookCount - 1; ++j) {
                g_Hooks[j] = g_Hooks[j + 1];
            }
            g_HookCount--;
            return MH_OK;
        }
    }
    return MH_ERROR_NOT_CREATED;
}
