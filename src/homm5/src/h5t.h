/* HoMM5 test harness (dinput8.dll proxy, test only - never shipped) */
#pragma once
#include <windows.h>
#include <stdint.h>

extern int g_test;
void dlog(const char *fmt, ...);
void patch_bytes(uint32_t at, const void *b, int n);
void patch_jmp(uint32_t at, void *to);
void *iat_hook_mod(HMODULE mod, const char *dll, const char *func, void *repl);
extern HWND g_hwnd;

/* testio.c */
void testio_init(void);
void testio_present(void *dev);
HRESULT testio_wrap_dinput(void *di);
void testio_cursor(POINT *p);   /* virtual cursor, client coordinates */
