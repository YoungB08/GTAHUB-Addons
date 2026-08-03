#include <windows.h>
#include "buffer.h"

#define MEMORY_BLOCK_SIZE 0x1000
#define MAX_BUFFER_SIZE   0x20

typedef struct _MEMORY_BLOCK {
    struct _MEMORY_BLOCK* pNext;
    PBUFFER               pFree;
    UINT                  usedCount;
} MEMORY_BLOCK, *PMEMORY_BLOCK;

static PMEMORY_BLOCK g_pMemoryBlocks = NULL;

void InitializeBuffer(void) {
    g_pMemoryBlocks = NULL;
}

void UninitializeBuffer(void) {
    PMEMORY_BLOCK pBlock = g_pMemoryBlocks;
    while (pBlock != NULL) {
        PMEMORY_BLOCK pNext = pBlock->pNext;
        VirtualFree(pBlock, 0, MEM_RELEASE);
        pBlock = pNext;
    }
    g_pMemoryBlocks = NULL;
}

void* AllocateBuffer(void* pOrigin) {
    (void)pOrigin;
    PMEMORY_BLOCK pBlock = g_pMemoryBlocks;
    while (pBlock != NULL && pBlock->pFree == NULL) {
        pBlock = pBlock->pNext;
    }

    if (pBlock == NULL) {
        pBlock = (PMEMORY_BLOCK)VirtualAlloc(NULL, MEMORY_BLOCK_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (pBlock == NULL) return NULL;

        pBlock->pNext = g_pMemoryBlocks;
        pBlock->usedCount = 0;

        PBUFFER pBuf = (PBUFFER)((ULONG_PTR)pBlock + sizeof(MEMORY_BLOCK));
        pBlock->pFree = pBuf;
        
        ULONG_PTR pLimit = (ULONG_PTR)pBlock + MEMORY_BLOCK_SIZE - MAX_BUFFER_SIZE;
        while ((ULONG_PTR)pBuf < pLimit) {
            pBuf->pNext = (PBUFFER)((ULONG_PTR)pBuf + MAX_BUFFER_SIZE);
            pBuf = pBuf->pNext;
        }
        pBuf->pNext = NULL;

        g_pMemoryBlocks = pBlock;
    }

    PBUFFER pResult = pBlock->pFree;
    pBlock->pFree = pResult->pNext;
    pBlock->usedCount++;
    return pResult;
}

void FreeBuffer(void* pBuffer) {
    if (pBuffer == NULL) return;
    PMEMORY_BLOCK pBlock = g_pMemoryBlocks;
    PMEMORY_BLOCK pPrev = NULL;
    while (pBlock != NULL) {
        if ((ULONG_PTR)pBuffer >= (ULONG_PTR)pBlock &&
            (ULONG_PTR)pBuffer < (ULONG_PTR)pBlock + MEMORY_BLOCK_SIZE) {
            PBUFFER pBuf = (PBUFFER)pBuffer;
            pBuf->pNext = pBlock->pFree;
            pBlock->pFree = pBuf;
            pBlock->usedCount--;

            if (pBlock->usedCount == 0) {
                if (pPrev != NULL) {
                    pPrev->pNext = pBlock->pNext;
                } else {
                    g_pMemoryBlocks = pBlock->pNext;
                }
                VirtualFree(pBlock, 0, MEM_RELEASE);
            }
            return;
        }
        pPrev = pBlock;
        pBlock = pBlock->pNext;
    }
}

BOOL IsExecutableAddress(void* pAddress) {
    MEMORY_BASIC_INFORMATION mi;
    if (VirtualQuery(pAddress, &mi, sizeof(mi)) == 0) return FALSE;
    return (mi.State == MEM_COMMIT) &&
           (mi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
}
