# AES-ECB through Windows CNG (bcrypt.dll), no third-party packages
import ctypes
from ctypes import wintypes as W
bc = ctypes.WinDLL('bcrypt')

class AES:
    def __init__(self, key):
        self.alg = W.HANDLE(); self.k = W.HANDLE()
        assert bc.BCryptOpenAlgorithmProvider(ctypes.byref(self.alg), 'AES', None, 0) == 0
        mode = 'ChainingModeECB'
        assert bc.BCryptSetProperty(self.alg, 'ChainingMode', ctypes.c_wchar_p(mode), (len(mode) + 1) * 2, 0) == 0
        kb = ctypes.create_string_buffer(key, len(key))
        assert bc.BCryptGenerateSymmetricKey(self.alg, ctypes.byref(self.k), None, 0, kb, len(key), 0) == 0
    def _run(self, f, data):
        inb = ctypes.create_string_buffer(bytes(data), len(data)); out = ctypes.create_string_buffer(len(data)); n = W.ULONG()
        r = f(self.k, inb, len(data), None, None, 0, out, len(data), ctypes.byref(n), 0)
        assert r == 0, hex(r & 0xffffffff)
        return out.raw[:n.value]
    def enc(self, data): return self._run(bc.BCryptEncrypt, data)
    def dec(self, data): return self._run(bc.BCryptDecrypt, data)
