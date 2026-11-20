#pragma once
#include <windows.h>
#include <stdint.h>
void logf_(const char *fmt, ...);
void patch_bytes(uint32_t at, const void *b, int n);
void hook_jmp(uint32_t at, void *to);
void *make_tramp(uint32_t at, int n);
const void *res_get(int id, DWORD *size);
extern int g_test;
/* text.c */
void text_init(void);
const char *tr_lookup(const char *s, uint32_t len, uint32_t *outlen);
void str_hook_init(void);
/* font.c */
void font_init(void);
void *iat_hook(const char *dll, const char *fn, void *nf);
void test_init(void);
void test_rehook(void);
void fakedd_init(void);
