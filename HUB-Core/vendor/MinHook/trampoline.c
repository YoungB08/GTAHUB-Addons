#include <windows.h>
#include <string.h>
#include "trampoline.h"
#include "buffer.h"

BOOL CreateTrampoline32(PTRAMPOLINE ct) {
    if (!ct || !ct->pTarget) return FALSE;

    DWORD oldProtect = 0;
    if (!VirtualProtect(ct->pTarget, 32, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return FALSE;
    }

    hde32s hs;
    UINT oldPos = 0;
    ct->patchSize = 0;

    while (ct->patchSize < 5) {
        BYTE* pOld = (BYTE*)ct->pTarget + oldPos;
        if (hde32_disasm(pOld, &hs) == 0 || (hs.flags & F_ERROR)) {
            DWORD unused = 0;
            VirtualProtect(ct->pTarget, 32, oldProtect, &unused);
            return FALSE;
        }
        oldPos += hs.len;
        ct->patchSize += hs.len;
    }

    BYTE* pTrampoline = (BYTE*)AllocateBuffer(ct->pTarget);
    if (pTrampoline == NULL) {
        DWORD unused = 0;
        VirtualProtect(ct->pTarget, 32, oldProtect, &unused);
        return FALSE;
    }

    memcpy(pTrampoline, ct->pTarget, ct->patchSize);

    BYTE* pJmpBack = pTrampoline + ct->patchSize;
    pJmpBack[0] = 0xE9; // JMP rel32
    *(DWORD*)(pJmpBack + 1) = (DWORD)((BYTE*)ct->pTarget + ct->patchSize) - (DWORD)(pJmpBack + 5);

    ct->pTrampoline = pTrampoline;
    memcpy(ct->backup, ct->pTarget, ct->patchSize);

    DWORD unused = 0;
    VirtualProtect(ct->pTarget, 32, oldProtect, &unused);
    return TRUE;
}
