/* H4JP: Heroes of Might and Magic IV Complete (GOG) Japanese patch.
   Loaded as wsock32.dll from the game folder; remove the DLL to get English back. */
#include "h4jp.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static HMODULE g_self;

#ifdef H4JP_TEST
/* 試験版だけ: H4JP_TEST で試験の仕組みを入れ、H4JP_LOG のファイルに記録を書く */
int g_test;
static FILE *g_log;
static CRITICAL_SECTION g_logcs;

void logf_(const char *fmt, ...)
{
    va_list a;
    if (!g_log) return;
    EnterCriticalSection(&g_logcs);
    va_start(a, fmt); vfprintf(g_log, fmt, a); va_end(a);
    fputc('\n', g_log); fflush(g_log);
    LeaveCriticalSection(&g_logcs);
}
#endif

void patch_bytes(uint32_t at, const void *b, int n)
{
    DWORD o;
    VirtualProtect((void *)at, n, PAGE_EXECUTE_READWRITE, &o);
    memcpy((void *)at, b, n);
    VirtualProtect((void *)at, n, o, &o);
    FlushInstructionCache(GetCurrentProcess(), (void *)at, n);
}

void hook_jmp(uint32_t at, void *to)
{
    uint8_t j[5]; j[0] = 0xE9; *(int32_t *)(j + 1) = (int32_t)((uint32_t)to - at - 5);
    patch_bytes(at, j, 5);
}

void *make_tramp(uint32_t at, int n)
{
    uint8_t *t = VirtualAlloc(0, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    memcpy(t, (void *)at, n);
    t[n] = 0xE9; *(int32_t *)(t + n + 1) = (int32_t)(at + n - ((uint32_t)t + n + 5));
    return t;
}

const void *res_get(int id, DWORD *size)
{
    HRSRC r = FindResourceA(g_self, MAKEINTRESOURCEA(id), "H4JP");
    HGLOBAL g;
    if (!r) { *size = 0; return 0; }
    g = LoadResource(g_self, r); *size = SizeofResource(g_self, r);
    return LockResource(g);
}

/* ---- IAT hooks ---- */
static void **iat_slot(HMODULE m, const char *dll, const char *fn)
{
    uint8_t *b = (uint8_t *)m;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(b + dd->VirtualAddress);
    for (; d->Name; d++) {
        IMAGE_THUNK_DATA *o, *t;
        if (_stricmp((char *)(b + d->Name), dll)) continue;
        o = (IMAGE_THUNK_DATA *)(b + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        t = (IMAGE_THUNK_DATA *)(b + d->FirstThunk);
        for (; o->u1.AddressOfData; o++, t++) {
            if (o->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            if (!strcmp((char *)((IMAGE_IMPORT_BY_NAME *)(b + o->u1.AddressOfData))->Name, fn)) return (void **)&t->u1.Function;
        }
    }
    return 0;
}
void *iat_hook(const char *dll, const char *fn, void *nf)
{
    void **s = iat_slot(GetModuleHandleA(0), dll, fn), *old;
    DWORD o;
    if (!s) { logf_("IAT %s!%s not found", dll, fn); return 0; }
    old = *s;
    VirtualProtect(s, 4, PAGE_READWRITE, &o); *s = nf; VirtualProtect(s, 4, o, &o);
    return old;
}


/* the GOG heroes4.exe (2003-01-24 build, no relocations) */
static int check_exe(void)
{
    IMAGE_DOS_HEADER *d = (IMAGE_DOS_HEADER *)0x400000;
    IMAGE_NT_HEADERS *n = (IMAGE_NT_HEADERS *)(0x400000 + d->e_lfanew);
    static const uint8_t rs[] = {0x64, 0xa1, 0x00, 0x00, 0x00, 0x00, 0x6a, 0xff, 0x68, 0xfb, 0x60, 0x96, 0x00};
    if (n->FileHeader.TimeDateStamp != 1040759937u) return 0;
    return !memcmp((void *)0x879420, rs, sizeof rs);
}

static void init(void)
{
#ifdef H4JP_TEST
    char b[16];
    g_test = GetEnvironmentVariableA("H4JP_TEST", b, sizeof b) > 0;
#endif
    if (!check_exe()) { logf_("unknown heroes4.exe, staying English"); return; }
    text_init();
    str_hook_init();
    font_init();
#ifdef H4JP_TEST
    if (g_test) test_init();
#endif
    logf_("init done (test=%d)", g_test);
}

/* ---- wsock32.dll proxy (the game imports WSOCK32 statically; most exports are forwarders) ---- */
#include "fwd.h"
static HMODULE g_real;
static FARPROC g_fp[4];
static const char *g_names[4] = { "getsockopt", "recv", "recvfrom", "setsockopt" };
static void load_real(void)
{
    char p[MAX_PATH]; int i;
    GetSystemDirectoryA(p, MAX_PATH); strcat(p, "\\wsock32.dll");
    g_real = LoadLibraryA(p);
    for (i = 0; i < 4; i++) g_fp[i] = g_real ? GetProcAddress(g_real, g_names[i]) : 0;
}
#define STUB(i, name) __declspec(naked) void name(void) { __asm { jmp dword ptr [g_fp + i * 4] } }
STUB(0, p_getsockopt) STUB(1, p_recv) STUB(2, p_recvfrom) STUB(3, p_setsockopt)

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID x)
{
    if (r == DLL_PROCESS_ATTACH) {
        char exe[MAX_PATH], *b;
        g_self = h;
        DisableThreadLibraryCalls(h);
#ifdef H4JP_TEST
        {   char lb[MAX_PATH];
            InitializeCriticalSection(&g_logcs);
            if (GetEnvironmentVariableA("H4JP_LOG", lb, sizeof lb)) g_log = fopen(lb, "w");
        }
#endif
        load_real();
        GetModuleFileNameA(0, exe, MAX_PATH);
        b = strrchr(exe, '\\'); b = b ? b + 1 : exe;
        logf_("loaded into %s", exe);
        if (!_stricmp(b, "heroes4.exe")) init();
    }
    return TRUE;
}
