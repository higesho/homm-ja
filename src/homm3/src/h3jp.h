#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

/* game structures (Heroes3.exe Complete 4.0 / SoD 3.2) */
typedef struct { uint8_t flag; char *s; uint32_t len, cap; } H3Str;
typedef struct { void *vt; H3Str *beg, *end, *cap; } H3StrVec;   /* +4 begin, +8 end, +0xc cap */

/* per-exe addresses (GOG Complete Heroes3.exe / HotA h3hota.exe = SoD 3.2) */
typedef struct {
    const char *name;
    uint32_t drawchar, drawline, textdraw, strwidth, linescount, totalwidth, maxlinewidth, maxwordwidth, maxlinewidthw, splitlines;
    uint32_t txt_table, txt_file;   /* text resource constructors (this, name, size, buf) */
    uint32_t vec_clear, vec_insert, str_assign, str_tidy, readstring, strncpy_;
    uint32_t t_bpp16, t_fullchk, t_fullvar, t_speedtest, t_intro;   /* test mode only */
    int hota;
} Addrs;
extern const Addrs *A;
/* A->hota = SoD 3.2 の番地の並び（h3hota.exe と、HD Mod がどの版でも動かす SoD 3.2 の exe）。
   g_hota = Horn of the Abyss そのもの（専用の TXT 一式と HotA.dat を持つ） */
extern int g_hota;

/* main.c */
#ifdef H3JP_TEST
extern int g_test;
void logf_(const char *fmt, ...);
#else
/* 配布版: 試験の分岐と記録は持たない（コンパイラが消す） */
#define g_test 0
#define logf_(...) ((void)0)
#endif
void hook_jmp(uint32_t at, void *to);
void *make_tramp(uint32_t at, int n);           /* copy n bytes from at + jmp back */
const void *res_get(int id, DWORD *size);
void patch_bytes(uint32_t at, const void *b, int n);

/* text.c */
void text_init(void);
const char *tr_lookup(const char *s, uint32_t len, uint32_t *outlen);
void txt_redirect_init(void);
void txt_hook_init(void);
const char *txt_redirect(const char *path);
const char *dat_redirect(const char *path, void *caller);     /* returns replacement path or NULL */

/* font.c */
void font_init(void);
void font_late_hooks(void);
int is_jp(const uint8_t *p);
const uint8_t *jp_glyph(int set, int code, int *w, int *h, int *bpr, int *adv);

/* subs.c */
void subs_init(void);

/* main.c */
void *iat_hook(const char *dll, const char *fn, void *nf);
void *api_hook(const char *dll, const char *fn, void *nf);

#ifdef H3JP_TEST
/* testio.c（試験版だけ） */
void test_init(void);

/* fakedd.c（試験版だけ） */
void fakedd_init(void);
int fakedd_shot(const char *path);
#endif
