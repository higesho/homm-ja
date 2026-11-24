/* H3JP: Heroes of Might and Magic III Complete (GOG) Japanese patch.
   Loaded as wsock32.dll from the game folder; remove the DLL to get English back. */
#include "h3jp.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <intrin.h>

int g_hota;
static HMODULE g_self;

#ifdef H3JP_TEST
/* 試験版だけ: H3JP_TEST で試験の仕組みを入れ、H3JP_LOG のファイルに記録を書く */
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
    HRSRC r = FindResourceA(g_self, MAKEINTRESOURCEA(id), "H3JP");
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

/* inline hook of an exported API (for callers that bypass the IAT, e.g. HD Mod) */
void *api_hook(const char *dll, const char *fn, void *nf)
{
    uint8_t *p = (uint8_t *)GetProcAddress(GetModuleHandleA(dll), fn);
    static const uint8_t pro[] = { 0x8B, 0xFF, 0x55, 0x8B, 0xEC };
    if (p && !memcmp(p, pro, 5)) {
        void *t = make_tramp((uint32_t)p, 5);
        hook_jmp((uint32_t)p, nf);
        return t;
    }
    if (p && p[0] == 0xFF && p[1] == 0x25) {          /* jmp [import] stub (forwards to win32u) */
        void *orig = **(void ***)(p + 2);
        hook_jmp((uint32_t)p, nf);
        return orig;
    }
    if (p) { logf_("api_hook %s!%s: unknown prologue, not hooked", dll, fn); return p; }
    logf_("api_hook %s!%s: unexpected prologue, using IAT", dll, fn);
    return iat_hook(dll, fn, nf);
}

/* ---- file redirect (translated TXT files) ---- */
static HANDLE (WINAPI *o_CreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static HANDLE WINAPI h_CreateFileA(LPCSTR p, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD c, DWORD f, HANDLE t)
{
    const char *r = p ? txt_redirect(p) : 0;
    if (!r && p && g_hota) r = dat_redirect(p, _ReturnAddress());
    if (g_test > 1) logf_("open %s%s%s", p ? p : "(null)", r ? " -> " : "", r ? r : "");
    return o_CreateFileA(r ? r : p, a, s, sa, c, f, t);
}
static HANDLE (WINAPI *o_CreateFileW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static HANDLE WINAPI h_CreateFileW(LPCWSTR p, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD c, DWORD f, HANDLE t)
{
    if (g_test > 1 && p) logf_("openW %ls", p);
    if (p && g_hota) {
        char pa[MAX_PATH * 2]; const char *r;
        WideCharToMultiByte(CP_ACP, 0, p, -1, pa, sizeof pa, 0, 0);
        r = dat_redirect(pa, _ReturnAddress());
        if (r) { WCHAR w[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, r, -1, w, MAX_PATH); return o_CreateFileW(w, a, s, sa, c, f, t); }
    }
    return o_CreateFileW(p, a, s, sa, c, f, t);
}
static DWORD (WINAPI *o_GetFileAttributesA)(LPCSTR);
static DWORD WINAPI h_GetFileAttributesA(LPCSTR p)
{
    const char *r = p ? txt_redirect(p) : 0;
    return o_GetFileAttributesA(r ? r : p);
}

/* ---- map / campaign strings: 0x4c6010 ReadString(stream ecx, H3String* edx) ---- */
static int (__fastcall *o_ReadString)(void *st, H3Str *s);
static int __fastcall h_ReadString(void *st, H3Str *s)
{
    int r = o_ReadString(st, s);
    if (r > 0 && s->s && s->len) {
        uint32_t n; const char *j = tr_lookup(s->s, s->len, &n);
        if (j) ((void (__fastcall *)(H3Str *, void *, const char *, int))A->str_assign)(s, 0, j, n);
#ifdef H3JP_TEST
        else if (g_test) logf_("MAPSTR|%.*s", (int)(s->len > 300 ? 300 : s->len), s->s);
#endif
    }
    return r;
}

#ifdef H3JP_TEST
/* 試験版だけ: 落ちたときの番地を記録する */
static LONG CALLBACK veh(EXCEPTION_POINTERS *e)
{
    static int n;
    if (e->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && n++ < 20) {
        HMODULE mod = 0; char mn[MAX_PATH] = "?";
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)e->ExceptionRecord->ExceptionAddress, &mod);
        if (mod) GetModuleFileNameA(mod, mn, MAX_PATH);
        logf_("module %s base %p", mn, mod);
        logf_("AV at %p (addr %p) eax=%08x ecx=%08x edx=%08x esp=%08x", e->ExceptionRecord->ExceptionAddress,
              (void *)e->ExceptionRecord->ExceptionInformation[1], e->ContextRecord->Eax, e->ContextRecord->Ecx,
              e->ContextRecord->Edx, e->ContextRecord->Esp);
        {   int i; uint32_t *sp = (uint32_t *)e->ContextRecord->Esp;
            for (i = 0; i < 24; i++) logf_("  [esp+%02x] %08x", i * 4, sp[i]); }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

/* 0x618a10 strncpy: same as CRT, but never leave half of a 2-byte EUC character at the cut
   (hero names are char[13], scenario names/descriptions are cut at 60/300 bytes, ...) */
static char *__cdecl h_strncpy(char *d, const char *s, size_t n)
{
    size_t i = 0, k;
    while (i < n && s[i]) { d[i] = s[i]; i++; }
    k = i;
    while (i < n) d[i++] = 0;
    if (k == n && n > 0 && s[k]) {           /* truncated: drop a dangling lead byte */
        size_t j = 0, jp = 0;
        for (j = 0; j < k; j++) if ((uint8_t)d[j] >= 0xA1) { jp = 1; break; }
        if (!jp) return d;                   /* plain English: original behaviour */
        d[n - 1] = 0;                        /* translated text: always terminate */
        k = n - 1; j = 0;
        while (j < k) {
            uint8_t c = (uint8_t)d[j];
            if (c >= 0xA1 && c <= 0xFE) { if (j + 1 >= k) { d[j] = 0; break; } j += 2; }
            else j++;
        }
    }
    return d;
}

static const Addrs g_addrs[] = {
    { "GOG Complete", 0x4b51a0, 0x4b5260, 0x4b5490, 0x4b57d0, 0x4b5820, 0x4b5920, 0x4b5990, 0x4b5a10, 0x4b5a80, 0x4b5b90,
      0x5bbe70, 0x5bbba0, 0x4b6010, 0x4af550, 0x404150, 0x4040f0, 0x4c6010, 0x618a10,
      0x601460, 0x4ed680, 0x6987b8, 0x4ee63b, 0x4ee7a6, 0 },
    { "HotA (SoD 3.2)", 0x4b4f00, 0x4b4fc0, 0x4b51f0, 0x4b5530, 0x4b5580, 0x4b5680, 0x4b56f0, 0x4b5770, 0x4b57e0, 0x4b58f0,
      0x5bc260, 0x5bbf90, 0x4b5d70, 0x4af250, 0x404180, 0x404130, 0x4c5cd0, 0x618fe0,
      0x601a20, 0x4edd10, 0x698808, 0x4eeccb, 0, 1 },
};
const Addrs *A;

static int game_ok(void)
{
    /* pick the address table whose code matches (prologues of ReadString and TextDraw) */
    static const uint8_t f1[] = { 0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x53, 0x56, 0x8b, 0xf1 };   /* ReadString */
    static const uint8_t f2[] = { 0x55, 0x8b, 0xec, 0x83, 0xec, 0x18, 0x8b, 0x55, 0x08 };        /* TextDraw */
    int i;
    __try {
        if (*(uint32_t *)0x400000 != 0x00905a4d) return 0;
        for (i = 0; i < (int)(sizeof g_addrs / sizeof g_addrs[0]); i++)
            if (!memcmp((void *)g_addrs[i].readstring, f1, sizeof f1) && !memcmp((void *)g_addrs[i].textdraw, f2, sizeof f2)) {
                A = &g_addrs[i]; logf_("exe: %s", A->name); return 1;
            }
    } __except (1) { return 0; }
    return 0;
}

/* h3hota.exe は HotA.dll を読み込む追加の入口から始まる。素の SoD 3.2 の exe（HD Mod が本編用の
   「Heroes3 HD.exe」にするものも同じ）は元の入口 0x61a884 から始まる。exe の名前では見分けない
   （HD Mod では名前を自由に付けられ、HotA は本編のフォルダに重ねて入れることも多い）。 */
static int detect_hota(void)
{
    uint8_t *b = (uint8_t *)0x400000; char p[MAX_PATH], *s;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    if (GetModuleHandleA("HotA.dll")) return 1;
    if (nt->OptionalHeader.AddressOfEntryPoint == 0x21a884) return 0;
    GetModuleFileNameA(0, p, MAX_PATH);
    s = strrchr(p, 92); if (!s) return 0;
    strcpy(s + 1, "HotA.dll");
    return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
}

static void init(void)
{
#ifdef H3JP_TEST
    char buf[MAX_PATH];
    if (GetEnvironmentVariableA("H3JP_TEST", buf, sizeof buf)) g_test = atoi(buf) ? atoi(buf) : 1;
#endif
    if (!game_ok()) { logf_("not the expected Heroes3.exe; patch disabled"); return; }
    g_hota = A->hota && detect_hota();
    logf_("HotA: %d, HD Mod: %d", g_hota, GetModuleHandleA("_hd3_.dll") != 0);
    text_init();
    font_init();
    hook_jmp(A->strncpy_, h_strncpy);
    o_ReadString = make_tramp(A->readstring, 6);
    hook_jmp(A->readstring, h_ReadString);
    txt_redirect_init();
    o_CreateFileA = A->hota ? api_hook("kernel32.dll", "CreateFileA", h_CreateFileA)
                            : iat_hook("kernel32.dll", "CreateFileA", h_CreateFileA);
    if (g_hota) o_CreateFileW = api_hook("kernel32.dll", "CreateFileW", h_CreateFileW);
    txt_hook_init();
    if (!A->hota) subs_init();     /* 字幕は Complete の exe だけ（HD Mod の下で掛けるとロゴの動画が止まる） */
    o_GetFileAttributesA = iat_hook("kernel32.dll", "GetFileAttributesA", h_GetFileAttributesA);
#ifdef H3JP_TEST
    if (g_test) { test_init(); AddVectoredExceptionHandler(1, veh); }
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
        g_self = h;
        DisableThreadLibraryCalls(h);
#ifdef H3JP_TEST
        {   char exe[MAX_PATH], lb[MAX_PATH];
            InitializeCriticalSection(&g_logcs);
            if (GetEnvironmentVariableA("H3JP_LOG", lb, sizeof lb)) g_log = fopen(lb, "w");
            GetModuleFileNameA(0, exe, MAX_PATH);
            logf_("loaded into %s", exe);
        }
#endif
        load_real();
        init();     /* exe の名前は問わない（中身は game_ok() で確かめる） */
    }
    return TRUE;
}
