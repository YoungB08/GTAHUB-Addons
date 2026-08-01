#pragma once
#include <cstdint>

#if defined(_WIN32) || defined(_WIN64)
    #define AMXAPI __stdcall
#else
    #define AMXAPI
#endif

typedef int32_t cell;
typedef uint32_t ucell;

enum {
    AMX_ERR_NONE = 0,
    AMX_ERR_EXIT,
    AMX_ERR_ASSERT,
    AMX_ERR_STACKERR,
    AMX_ERR_BOUNDS,
    AMX_ERR_MEMLOW,
    AMX_ERR_INVHMEM,
    AMX_ERR_INVINSTR,
    AMX_ERR_INVNONT,
    AMX_ERR_INVARG,
    AMX_ERR_NOTFOUND,
    AMX_ERR_INIT,
    AMX_ERR_INDEX,
    AMX_ERR_DEBUG,
    AMX_ERR_INIT_JIT,
    AMX_ERR_PARAMS,
    AMX_ERR_DOMAIN,
    AMX_ERR_GENERAL,
    AMX_ERR_NUMERIC,
    AMX_ERR_SLEEP,
    AMX_ERR_INVSTATE,

    AMX_ERR_MEMORY = 16,
    AMX_ERR_FORMAT = 18,
    AMX_ERR_VERSION = 19,
    AMX_ERR_NATIVE = 20,
    AMX_ERR_TRACKER = 21,
    AMX_ERR_UNKNOWN = 22
};

struct AMX;

typedef cell (AMXAPI *AMX_NATIVE)(struct AMX *amx, const cell *params);
typedef int (AMXAPI *AMX_CALLBACK)(struct AMX *amx, cell index, cell *result, const cell *params);
typedef int (AMXAPI *AMX_DEBUG)(struct AMX *amx);

typedef struct {
    const char *name;
    AMX_NATIVE func;
} AMX_NATIVE_INFO;

#pragma pack(push, 1)
struct AMX_FUNCINFO {
    uint32_t address;
    char name[32];
};

struct AMX_HEADER {
    int32_t size;
    uint16_t magic;
    char file_version;
    char amx_version;
    int16_t flags;
    int16_t defsize;
    int32_t cod;
    int32_t dat;
    int32_t hea;
    int32_t stp;
    int32_t cip;
    int32_t publics;
    int32_t natives;
    int32_t libraries;
    int32_t pubvars;
    int32_t tags;
    int32_t nametable;
};

struct AMX {
    uint8_t* base;
    uint8_t* data;
    AMX_CALLBACK callback;
    AMX_NATIVE debug;
    cell cip;
    cell frm;
    cell hea;
    cell hlw;
    cell stk;
    cell stp;
    int flags;
    long usertags[4];
    void* userdata[4];
    int error;
    int paramcount;
    cell pri;
    cell alt;
    cell reset_stk;
    cell reset_hea;
    AMX_NATIVE sysreq_d;
};
#pragma pack(pop)
