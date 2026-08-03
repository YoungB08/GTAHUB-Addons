#include <string.h>
#include "hde32.h"
#include "table32.h"

unsigned int hde32_disasm(const void *code, hde32s *hs) {
    if (!code || !hs) return 0;
    uint8_t x, *p = (uint8_t *)code;
    uint8_t c, pref = 0;
    uint8_t *p0 = p;

    memset(hs, 0, sizeof(hde32s));
    c = *p++;

    if (c == 0xf3) {
        hs->p_rep = c;
        hs->flags |= F_PREFIX_REPNZ;
        c = *p++;
    } else if (c == 0xf2) {
        hs->p_rep = c;
        hs->flags |= F_PREFIX_REPZ;
        c = *p++;
    }

    if (c == 0xf0) {
        hs->p_lock = c;
        hs->flags |= F_PREFIX_LOCK;
        c = *p++;
    }

    if (c == 0x26 || c == 0x2e || c == 0x36 || c == 0x3e || c == 0x64 || c == 0x65) {
        hs->p_seg = c;
        c = *p++;
    }

    if (c == 0x66) {
        hs->p_66 = c;
        hs->flags |= F_PREFIX_66;
        c = *p++;
    }

    if (c == 0x67) {
        hs->p_67 = c;
        hs->flags |= F_PREFIX_67;
        c = *p++;
    }

    hs->opcode = c;

    if (c == 0x0f) {
        c = *p++;
        hs->opcode2 = c;
    }

    x = hde32_table[c];
    if (x & 0x01) {
        hs->modrm = *p++;
        hs->modrm_mod = (hs->modrm & 0xc0) >> 6;
        hs->modrm_reg = (hs->modrm & 0x38) >> 3;
        hs->modrm_rm  = hs->modrm & 0x07;
        hs->flags |= F_MODRM;

        if (hs->modrm_mod != 3 && hs->modrm_rm == 4) {
            hs->sib = *p++;
            hs->sib_scale = (hs->sib & 0xc0) >> 6;
            hs->sib_index = (hs->sib & 0x38) >> 3;
            hs->sib_base  = hs->sib & 0x07;
            hs->flags |= F_SIB;
        }

        if (hs->modrm_mod == 1) {
            hs->disp.disp8 = *p++;
            hs->flags |= F_DISP8;
        } else if (hs->modrm_mod == 2 || (hs->modrm_mod == 0 && hs->modrm_rm == 5)) {
            hs->disp.disp32 = *(uint32_t *)p;
            p += 4;
            hs->flags |= F_DISP32;
        }
    }

    if (c == 0xe8 || c == 0xe9) {
        hs->imm.imm32 = *(uint32_t *)p;
        p += 4;
        hs->flags |= F_IMM32 | F_RELATIVE;
    } else if (c == 0xeb) {
        hs->imm.imm8 = *p++;
        hs->flags |= F_IMM8 | F_RELATIVE;
    } else if (c == 0x68) {
        hs->imm.imm32 = *(uint32_t *)p;
        p += 4;
        hs->flags |= F_IMM32;
    } else if (c == 0x6a) {
        hs->imm.imm8 = *p++;
        hs->flags |= F_IMM8;
    } else if (c >= 0xb8 && c <= 0xbf) {
        hs->imm.imm32 = *(uint32_t *)p;
        p += 4;
        hs->flags |= F_IMM32;
    } else if (c >= 0xb0 && c <= 0xb7) {
        hs->imm.imm8 = *p++;
        hs->flags |= F_IMM8;
    }

    hs->len = (uint8_t)(p - p0);
    return hs->len;
}
