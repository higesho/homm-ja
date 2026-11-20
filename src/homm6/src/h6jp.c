/*
 * Might & Magic Heroes VI 日本語化パッチ（実行時部分）
 * ゲームの exe が読み込む msimg32.dll の代理としてゲームのフォルダに置く。ゲームのファイルは書き換えない。
 *  - 英語（0x409）のときは Localization\Game.orc の翻訳ファイルを読まない処理になっているので、
 *    その分岐（je）を外して、英語でも Game.orc（能力・クリーチャー・建物などのデータの文）の訳を読ませる。
 *  - 書き換える前に元のバイト列を確かめ、違う版の exe なら何もしない。
 */
#include <windows.h>
#include <string.h>

void proxy_init(void);

/* cmp [ebp-0x80],0x409 ; je +0x137 ; mov [ebp-0x3c],"\Localization\Game.orc\" */
static const unsigned char want[] = {
    0x81, 0x7D, 0x80, 0x09, 0x04, 0x00, 0x00,
    0x0F, 0x84, 0x37, 0x01, 0x00, 0x00,
    0xC7, 0x45, 0xC4, 0xB0, 0x00, 0xE8, 0x01 };

static void *find(unsigned char *base, size_t size)
{
    size_t i;
    if (size < sizeof want) return 0;
    for (i = 0; i + sizeof want <= size; i++)
        if (base[i] == want[0] && !memcmp(base + i, want, sizeof want)) return base + i;
    return 0;
}

static void patch(void)
{
    unsigned char *base = (unsigned char *)GetModuleHandleA(0), *p;
    IMAGE_NT_HEADERS *nh = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_SECTION_HEADER *sh = IMAGE_FIRST_SECTION(nh);
    DWORD old;
    p = (unsigned char *)0x141e4ff;                         /* Steam 版 2.1.1 */
    if (memcmp(p, want, sizeof want)) p = find(base + sh->VirtualAddress, sh->Misc.VirtualSize);
    if (!p) return;
    VirtualProtect(p + 7, 6, PAGE_EXECUTE_READWRITE, &old);
    memset(p + 7, 0x90, 6);
    VirtualProtect(p + 7, 6, old, &old);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        proxy_init();
        __try { patch(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    return TRUE;
}
