/* msimg32.dll の代理: システムの msimg32.dll（SysWOW64）の全関数を中継する（tools/gen_proxy.py で生成） */
#include <windows.h>

#define FUNCS(X) \
    X(AlphaBlend) X(DllInitialize) X(GradientFill) X(TransparentBlt) \
    X(vSetDdrawflag)

#define DECL(n) void *pf_##n;
FUNCS(DECL)

static HMODULE g_real;

void proxy_init(void)
{
    char path[MAX_PATH];
    UINT n = GetSystemDirectoryA(path, MAX_PATH);
    lstrcpyA(path + n, "\\msimg32.dll");
    g_real = LoadLibraryA(path);
    if (!g_real) return;
#define LOAD(n) pf_##n = (void*)GetProcAddress(g_real, #n);
    FUNCS(LOAD)
}

#define STUB(n) __declspec(naked) void p_##n(void) { __asm { jmp dword ptr [pf_##n] } }
FUNCS(STUB)
