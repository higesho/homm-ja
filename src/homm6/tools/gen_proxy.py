# src/proxy.c と src/msimg32.def を作る: システムの msimg32.dll（32 ビット = SysWOW64）の全関数を中継する
import os, pefile
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
DLL = 'msimg32'
p = pefile.PE(r'C:\Windows\SysWOW64\%s.dll' % DLL)
names = [e.name.decode() for e in p.DIRECTORY_ENTRY_EXPORT.symbols if e.name]
L = ['/* %s.dll の代理: システムの %s.dll（SysWOW64）の全関数を中継する（tools/gen_proxy.py で生成） */' % (DLL, DLL),
     '#include <windows.h>', '', '#define FUNCS(X) \\']
for i in range(0, len(names), 4):
    L.append('    ' + ' '.join('X(%s)' % n for n in names[i:i + 4]) + ' \\')
L[-1] = L[-1][:-2]
L += ['', '#define DECL(n) void *pf_##n;', 'FUNCS(DECL)', '', 'static HMODULE g_real;', '',
      'void proxy_init(void)', '{', '    char path[MAX_PATH];', '    UINT n = GetSystemDirectoryA(path, MAX_PATH);',
      '    lstrcpyA(path + n, "\\\\%s.dll");' % DLL, '    g_real = LoadLibraryA(path);', '    if (!g_real) return;',
      '#define LOAD(n) pf_##n = (void*)GetProcAddress(g_real, #n);', '    FUNCS(LOAD)', '}', '',
      '#define STUB(n) __declspec(naked) void p_##n(void) { __asm { jmp dword ptr [pf_##n] } }', 'FUNCS(STUB)', '']
os.makedirs(os.path.join(ROOT, 'src'), exist_ok=True)
open(os.path.join(ROOT, 'src', 'proxy.c'), 'w', encoding='utf-8').write('\n'.join(L))
open(os.path.join(ROOT, 'src', DLL + '.def'), 'w').write('LIBRARY %s\nEXPORTS\n' % DLL + ''.join('    %s=p_%s\n' % (n, n) for n in names))
print(len(names), 'functions')
