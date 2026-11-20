# read bytes from the running game:  python tools/peek.py <hexaddr> [len]   ;  python tools/peek.py mods
import sys, ctypes, ctypes.wintypes as W, subprocess
k = ctypes.windll.kernel32
out = subprocess.run(['tasklist', '/fi', 'imagename eq Might & Magic Heroes VI.exe', '/fo', 'csv', '/nh'], capture_output=True, text=True).stdout
pid = int(out.split(',')[1].strip('"'))
h = k.OpenProcess(0x0410 | 0x0008, False, pid)
if sys.argv[1] == 'mods':
    ps = ctypes.windll.psapi
    arr = (W.HMODULE * 1024)(); need = W.DWORD()
    ps.EnumProcessModulesEx(h, arr, ctypes.sizeof(arr), ctypes.byref(need), 3)
    for m in arr[:need.value // ctypes.sizeof(W.HMODULE)]:
        b = ctypes.create_unicode_buffer(512); ps.GetModuleFileNameExW(h, ctypes.c_void_p(m), b, 512)
        if len(sys.argv) < 3 or sys.argv[2].lower() in b.value.lower(): print(hex(m or 0), b.value)
else:
    a = int(sys.argv[1], 16); n = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x20
    buf = ctypes.create_string_buffer(n); r = ctypes.c_size_t()
    k.ReadProcessMemory(h, ctypes.c_void_p(a), buf, n, ctypes.byref(r))
    print(buf.raw[:r.value].hex(' '))
