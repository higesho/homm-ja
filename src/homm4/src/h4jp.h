#pragma once
#include <windows.h>
#include <stdint.h>
#ifdef H4JP_TEST
void logf_(const char *fmt, ...);
extern int g_test;
#else
/* 配布版: 試験の分岐と記録は持たない（コンパイラが消す） */
#define logf_(...) ((void)0)
#define g_test 0
#endif
void patch_bytes(uint32_t at, const void *b, int n);
void hook_jmp(uint32_t at, void *to);
void *make_tramp(uint32_t at, int n);
const void *res_get(int id, DWORD *size);
/* text.c */
void text_init(void);
const char *tr_lookup(const char *s, uint32_t len, uint32_t *outlen);
void str_hook_init(void);
/* font.c */
void font_init(void);
void *iat_hook(const char *dll, const char *fn, void *nf);
#ifdef H4JP_TEST
/* testio.c / fakedd.c（試験版だけ） */
void test_init(void);
void test_rehook(void);
void fakedd_init(void);
#endif
