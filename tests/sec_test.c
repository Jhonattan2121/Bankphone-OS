#include "../init/sec.h"
#include <stdio.h>
#include <string.h>
int main(void) {
    uint8_t h[32]; char o[65]; int bad = 0;
    sha256((const uint8_t *)"abc", 3, h); for (int i = 0; i < 32; i++) sprintf(o + 2 * i, "%02x", h[i]);
    if (strcmp(o, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")) { puts("FAIL sha256 abc"); bad++; } else puts("PASS sha256 abc");
    uint8_t salt[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}; pin_set("123456", salt);
    puts(pin_check("123456", 100) == 0 ? "PASS pin certo" : (bad++, "FAIL pin certo"));
    int r = 0; for (int i = 0; i < 4; i++) r |= pin_check("000000", 100) != 1;
    puts(!r ? "PASS pin errado x4" : (bad++, "FAIL errado"));
    puts(pin_check("000000", 100) == 1 && pin_locked_s(100) > 0 ? "PASS 5o erro bloqueia" : (bad++, "FAIL bloqueio"));
    puts(pin_check("123456", 101) == 2 ? "PASS bloqueado ate passar tempo" : (bad++, "FAIL bloqueado"));
    puts(pin_check("123456", 200) == 0 ? "PASS libera depois" : (bad++, "FAIL libera"));
    char b[200]; pin_serialize(b, 200); Pin keep = PIN; memset(&PIN, 0, sizeof PIN);
    puts(pin_deserialize(b) && !memcmp(PIN.hash, keep.hash, 32) ? "PASS serializa" : (bad++, "FAIL serializa"));

    /* ---- the PIN lockout and the clock. Reproduced on 32932dd. ----
     * ---- bloqueio do PIN e o relógio. Reproduzido no 32932dd. ---- */
#define T(name, cond) do { if (cond) puts("PASS " name); else { bad++; puts("FAIL " name); } } while (0)
    {   /* the lock is saved as an absolute time; plat_now() switches between wall clock and uptime. A lock written under one clock
         * and read under the other lasted decades (55.8 years in the reproduction).
         * o bloqueio é gravado como hora absoluta; o plat_now() alterna entre relógio real e uptime. Um bloqueio gravado num relógio e lido
         * no outro durava décadas (55,8 anos na reprodução). */
        char line[256];
        uint8_t sl[16] = {9}; pin_set("123456", sl);
        for (int i = 0; i < 5; i++) pin_check("000000", 1759468800);
        pin_serialize(line, sizeof line); memset(&PIN, 0, sizeof PIN); pin_deserialize(line);
        T("relogio real -> uptime: o bloqueio gravado nao dura mais que o maximo legitimo (1920 s)", pin_locked_s(5) <= 1920);
        T("relogio real -> uptime: o PIN certo volta a funcionar depois de 1920 s", pin_check("123456", 5) == 2 && pin_check("123456", 5 + 1920) == 0);

        pin_set("123456", sl);                                   /* uptime of 40 days, then a reboot / 40 dias de uptime, depois um reboot */
        for (int i = 0; i < 5; i++) pin_check("000000", 3456000);
        pin_serialize(line, sizeof line); memset(&PIN, 0, sizeof PIN); pin_deserialize(line);
        T("uptime longo -> reboot: bloqueio limitado a 1920 s", pin_locked_s(5) <= 1920 && pin_check("123456", 5 + 1920) == 0);

        /* a legitimate lock is not shortened: 5 errors = 30 s, doubling to 1920 s / um bloqueio legítimo não encurta */
        pin_set("123456", sl);
        for (int i = 0; i < 5; i++) pin_check("000000", 1000);
        T("5 erros: bloqueio de 30 s exatos", pin_locked_s(1000) == 30 && pin_check("123456", 1029) == 2 && pin_locked_s(1029) == 1);
        T("o PIN certo passa quando os 30 s acabam", pin_check("123456", 1030) == 0);
        pin_set("123456", sl); int64_t now = 2000; int64_t want[] = { 30, 60, 120, 240, 480, 960, 1920, 1920, 1920 };
        int escalation_ok = 1;
        for (int e = 0; e < 9; e++) {
            for (int g = 0; g < (e == 0 ? 5 : 1); g++) pin_check("000000", now);
            if (pin_locked_s(now) != want[e]) escalation_ok = 0;
            now += want[e];
        }
        T("a escalada 30,60,...,1920 s continua exata (o limite nao encurta nada legitimo)", escalation_ok);
        pin_set("123456", sl);                                   /* the anchored deadline does not slide / o prazo ancorado não desliza */
        for (int i = 0; i < 5; i++) pin_check("000000", 1759468800);
        pin_serialize(line, sizeof line); memset(&PIN, 0, sizeof PIN); pin_deserialize(line);
        pin_locked_s(5);                                          /* first sight anchors it at 5 + 1920 / a primeira vista ancora em 5 + 1920 */
        T("o prazo ancorado nao desliza: consultas e tentativas erradas no meio nao o empurram", pin_locked_s(1000) == 925 && pin_check("123456", 1500) == 2 && pin_locked_s(1900) == 25 && pin_check("123456", 1925) == 0);
        pin_set("123456", sl);                                   /* the clamp never lengthens either / o limite também nunca alonga */
        for (int i = 0; i < 5; i++) pin_check("000000", 500);
        T("o limite nao alonga: bloqueio de 30 s segue 30 s", pin_locked_s(500) == 30);
    }
    {   /* a clock the code cannot take: refused, never computed (signed overflow is undefined behaviour) / relógio que o código não aguenta: recusado, nunca calculado */
        uint8_t sl[16] = {7}; pin_set("123456", sl);
        const int64_t BAD[] = { -1, INT64_MIN, INT64_MIN + 1, 100000000001LL, INT64_MAX - 10, INT64_MAX };
        int refused = 1, locked_s_ok = 1, unchanged = 1;
        for (unsigned i = 0; i < sizeof BAD / sizeof *BAD; i++) {
            int fails0 = PIN.fails; int64_t lu0 = PIN.locked_until;
            if (pin_check("123456", BAD[i]) != 2 || pin_check("000000", BAD[i]) != 2) refused = 0;
            if (pin_locked_s(BAD[i]) != 1920) locked_s_ok = 0;
            if (PIN.fails != fails0 || PIN.locked_until != lu0) unchanged = 0;
        }
        T("relogio invalido: pin_check recusa (2), certo ou errado", refused);
        T("relogio invalido: pin_locked_s nao estoura e diz o maximo", locked_s_ok);
        T("relogio invalido: nada muda (contador e bloqueio intactos)", unchanged);
        T("depois do relogio invalido o PIN certo funciona em hora valida", pin_check("123456", 100) == 0);
        pin_set("123456", sl);
        for (int i = 0; i < 5; i++) pin_check("000000", INT64_MAX - 10);
        T("relogio perto do INT64_MAX nao desliga o bloqueio em silencio", PIN.fails == 0 && pin_check("123456", 50) == 0);
    }
    {   /* the failed-try counter comes from an unauthenticated file: saturate it, never overflow / o contador vem de arquivo não autenticado: satura, nunca estoura */
        uint8_t sl[16] = {5}; pin_set("123456", sl);
        char line[256], hx[33], hh[65];
        for (int i = 0; i < 16; i++) sprintf(hx + 2 * i, "%02x", PIN.salt[i]);
        for (int i = 0; i < 32; i++) sprintf(hh + 2 * i, "%02x", PIN.hash[i]);
        snprintf(line, sizeof line, "P 1 2147483647 0 %s %s", hx, hh);
        T("contador INT_MAX e carregado", pin_deserialize(line));
        T("PIN errado com contador INT_MAX nao estoura e continua bloqueando", pin_check("000000", 100) == 1 && pin_locked_s(100) > 0 && pin_locked_s(100) <= 1920);
        snprintf(line, sizeof line, "P 1 -2147483648 0 %s %s", hx, hh);
        pin_deserialize(line);
        for (int i = 0; i < 5; i++) pin_check("000000", 100);
        T("contador negativo nao da tentativas infinitas: o bloqueio comeca", pin_locked_s(100) > 0);
    }
    {   /* the authorization grant and a clock outside the range / a concessão de autorização e um relógio fora da faixa */
        uint8_t sl[16] = {3}; pin_set("123456", sl);
        auth_clear(); auth_expect("tx1");
        T("PIN certo com relogio invalido nao emite concessao", pin_check("123456", -100) == 2 && auth_peek("tx1", 100) == L_NONE);
        auth_clear(); auth_expect("tx1"); pin_check("123456", 100);
        T("concessao valida emitida em hora valida", auth_peek("tx1", 100) == L_PIN);
        T("auth_peek com relogio gigante nao estoura e nao concede", auth_peek("tx1", INT64_MAX) == L_NONE);
        auth_clear(); auth_expect("tx1"); pin_check("123456", 100);
        T("auth_confirm com relogio negativo nao estoura e nao sobe", auth_confirm("tx1", INT64_MIN) == 0);
        auth_clear();
    }
#undef T

    puts(bad ? "FALHAS" : "TODOS OK"); return bad;
}
