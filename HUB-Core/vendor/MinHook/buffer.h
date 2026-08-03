#pragma once
#include <windows.h>

typedef struct _BUFFER {
    void*  pAddress;
    struct _BUFFER* pNext;
} BUFFER, *PBUFFER;

void  InitializeBuffer(void);
void  UninitializeBuffer(void);
void* AllocateBuffer(void* pOrigin);
void  FreeBuffer(void* pBuffer);
BOOL  IsExecutableAddress(void* pAddress);
