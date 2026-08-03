#pragma once
#include <windows.h>
#include "hde32.h"

typedef struct _TRAMPOLINE {
    void*  pTarget;
    void*  pDetour;
    void*  pTrampoline;
    UINT   patchSize;
    BYTE   backup[8];
} TRAMPOLINE, *PTRAMPOLINE;

BOOL CreateTrampoline32(PTRAMPOLINE ct);
