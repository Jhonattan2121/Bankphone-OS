#include "../init/sec.h"
#include <stdio.h>
#include <string.h>

/* Security tests. Each line printed as PASS/FAIL is one check (the Makefile counts the PASS lines).
 * Testes de segurança. Cada linha PASS/FAIL é uma verificação (o Makefile conta as linhas PASS).
 * Expected values of the primitives come from RFC 7914 / RFC 4231 / RFC 5869 and from an independent
 * implementation (OpenSSL via Python hashlib/hmac), not from this code.
 * Os valores esperados das primitivas vêm das RFCs 7914 / 4231 / 5869 e de uma implementação
 * independente (OpenSSL via hashlib/hmac do Python), não deste código. */

static int bad;
static void T(const char *name, int ok) { printf("%s %s\n", ok ? "PASS" : "FAIL", name); if (!ok) bad++; }

static void hex(char *o, const uint8_t *b, size_t n) { for (size_t i = 0; i < n; i++) sprintf(o + 2 * i, "%02x", b[i]); }
static int eqhex(const uint8_t *b, size_t n, const char *want) { char o[256]; hex(o, b, n); return !strcmp(o, want); }

int main(void) {
    uint8_t h[32], o[64];
    char s[65];

    /* ---------- SHA-256, incremental and one-shot / incremental e direto ---------- */
    sha256((const uint8_t *)"abc", 3, h); hex(s, h, 32);
    T("sha256 abc", !strcmp(s, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    {
        uint8_t msg[300]; for (int i = 0; i < 300; i++) msg[i] = (uint8_t)(i * 7 + 3);
        int diff = 0;
        for (size_t n = 0; n <= 300; n++) for (size_t cut = 0; cut <= n; cut += 7) {
            uint8_t a[32], b[32]; Sha256 c; sha256(msg, n, a);
            sha256_init(&c); sha256_update(&c, msg, cut); sha256_update(&c, msg + cut, n - cut); sha256_final(&c, b);
            diff += memcmp(a, b, 32) != 0;
        }
        T("sha256 incremental igual ao direto (todos os tamanhos e cortes)", diff == 0);
    }

    /* ---------- HMAC, PBKDF2, HKDF ---------- */
    hmac_sha256((const uint8_t *)"Jefe", 4, (const uint8_t *)"what do ya want for nothing?", 28, h);
    T("hmac-sha256 RFC 4231 caso 2", eqhex(h, 32, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"));
    { uint8_t k[131]; memset(k, 0xaa, sizeof k);
      hmac_sha256(k, sizeof k, (const uint8_t *)"Test Using Larger Than Block-Size Key - Hash Key First", 54, h);
      T("hmac-sha256 chave maior que o bloco (RFC 4231 caso 6)", eqhex(h, 32, "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54")); }
    pbkdf2_sha256((const uint8_t *)"passwd", 6, (const uint8_t *)"salt", 4, 1, o, 64);
    T("pbkdf2-sha256 RFC 7914 (c=1)", eqhex(o, 64, "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783"));
    pbkdf2_sha256((const uint8_t *)"Password", 8, (const uint8_t *)"NaCl", 4, 80000, o, 64);
    T("pbkdf2-sha256 RFC 7914 (c=80000)", eqhex(o, 64, "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56a1d425a1225833549adb841b51c9b3176a272bdebba1d078478f62b397f33c8d"));
    { uint8_t ikm[22], salt[13], info[10], okm[42]; memset(ikm, 0x0b, 22);
      for (int i = 0; i < 13; i++) salt[i] = (uint8_t)i;
      for (int i = 0; i < 10; i++) info[i] = (uint8_t)(0xf0 + i);
      T("hkdf-sha256 RFC 5869 caso 1", hkdf_sha256(ikm, 22, salt, 13, info, 10, okm, 42) == 0 && eqhex(okm, 42, "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865"));
      T("hkdf-sha256 RFC 5869 caso 3 (sem sal e sem info)", hkdf_sha256(ikm, 22, NULL, 0, NULL, 0, okm, 42) == 0 && eqhex(okm, 42, "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d9d201395faa4b61a96c8"));
      T("hkdf recusa saida maior que 255*32", hkdf_sha256(ikm, 22, NULL, 0, NULL, 0, okm, 255 * 32 + 1) == -1); }

    /* ---------- scrypt, RFC 7914 ---------- */
    T("scrypt RFC 7914 vetor 1 (N=16)", scrypt_kdf((const uint8_t *)"", 0, (const uint8_t *)"", 0, 4, 1, 1, o, 64) == 0 && eqhex(o, 64, "77d6576238657b203b19ca42c18a0497f16b4844e3074ae8dfdffa3fede21442fcd0069ded0948f8326a753a0fc81f17e8d3e0fb2e0d3628cf35e20c38d18906"));
    T("scrypt RFC 7914 vetor 2 (N=1024, r=8, p=16)", scrypt_kdf((const uint8_t *)"password", 8, (const uint8_t *)"NaCl", 4, 10, 8, 16, o, 64) == 0 && eqhex(o, 64, "fdbabe1c9d3472007856e7190d01e9fe7c6ad7cbc8237830e77376634b3731622eaf30d92e22a3886ff109279d9830dac727afb94a83ee6d8360cbdfa2cc0640"));
    T("scrypt RFC 7914 vetor 3 (N=16384, r=8)", scrypt_kdf((const uint8_t *)"pleaseletmein", 13, (const uint8_t *)"SodiumChloride", 14, 14, 8, 1, o, 64) == 0 && eqhex(o, 64, "7023bdcb3afd7348461c06cd81fd38ebfda8fbba904f8e3ea9b543f6545da1f2d5432955613f0fcf62d49705242a9af9e61e85dc0d651e40dfcf017b45575887"));
    T("scrypt recusa N=1 (log2n 0)", scrypt_kdf((const uint8_t *)"a", 1, (const uint8_t *)"s", 1, 0, 8, 1, o, 32) == -1);
    T("scrypt recusa r=0", scrypt_kdf((const uint8_t *)"a", 1, (const uint8_t *)"s", 1, 10, 0, 1, o, 32) == -1);
    T("scrypt recusa p=0", scrypt_kdf((const uint8_t *)"a", 1, (const uint8_t *)"s", 1, 10, 8, 0, o, 32) == -1);
    T("scrypt recusa memoria acima do teto (N=2^20, r=8 = 1 GiB)", scrypt_kdf((const uint8_t *)"a", 1, (const uint8_t *)"s", 1, 20, 8, 1, o, 32) == -1);
    T("scrypt_params_ok: aceita o custo padrao e recusa lixo", scrypt_params_ok(PIN_DEFAULT_LOG2N, PIN_DEFAULT_R, PIN_DEFAULT_P) && !scrypt_params_ok(64, 8, 1) && !scrypt_params_ok(10, 100, 1));

    /* ---------- constant-time compare and wipe / comparação e apagar ---------- */
    { uint8_t a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 4}, c[4] = {1, 2, 3, 5};
      T("sec_ct_eq igual e diferente", sec_ct_eq(a, b, 4) && !sec_ct_eq(a, c, 4));
      sec_wipe(a, 4); T("sec_wipe zera", !a[0] && !a[1] && !a[2] && !a[3]); }

    /* ---------- PIN: the behaviour that already existed, at a small cost so tests are fast ----------
     * ---------- PIN: o comportamento que já existia, com custo pequeno para o teste ser rápido ---------- */
    pin_kdf_cost(10, 8, 1);
    uint8_t salt[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    T("pin_set devolve 0", pin_set("123456", salt) == 0);
    T("pin certo", pin_check("123456", 100) == 0);
    { int r = 0; for (int i = 0; i < 4; i++) r |= pin_check("000000", 100) != 1; T("pin errado x4", !r); }
    T("5o erro bloqueia", pin_check("000000", 100) == 1 && pin_locked_s(100) > 0);
    T("bloqueado ate passar tempo", pin_check("123456", 101) == 2);
    T("libera depois", pin_check("123456", 200) == 0);
    { char b[200]; pin_serialize(b, 200); Pin keep = PIN; memset(&PIN, 0, sizeof PIN);
      T("serializa", pin_deserialize(b) && !memcmp(PIN.hash, keep.hash, 32) && PIN.ver == 2 && PIN.log2n == keep.log2n && PIN.r == keep.r && PIN.p == keep.p); }

    /* ---------- PIN v2: the verifier and the state key / o verificador e a chave do estado ---------- */
    {
        uint8_t v[32], k[32];
        T("pin_derive_v2 bate com a referencia independente (verificador)", pin_derive_v2("123456", salt, 10, 8, 1, v, k) == 0 && eqhex(v, 32, "60b1bfb1293ec59101b6ae1e6cc216d081d2a79e864df6eb16c2fb256e0f9264"));
        T("pin_derive_v2 bate com a referencia independente (chave do estado)", eqhex(k, 32, "a983137d6fdaadd4bda22f92c9ae759c6112bc6e5ef063c6da7e74fcf117ad72"));
        T("verificador e chave do estado sao diferentes", memcmp(v, k, 32) != 0);
        uint8_t salt2[16]; memcpy(salt2, salt, 16); salt2[0] ^= 1;
        uint8_t v2[32], k2[32]; pin_derive_v2("123456", salt2, 10, 8, 1, v2, k2);
        T("sal diferente muda verificador e chave", memcmp(v, v2, 32) && memcmp(k, k2, 32));
        uint8_t v3[32], k3[32]; pin_derive_v2("123457", salt, 10, 8, 1, v3, k3);
        T("PIN diferente muda verificador e chave", memcmp(v, v3, 32) && memcmp(k, k3, 32));
        uint8_t v4[32], k4[32]; pin_derive_v2("123456", salt, 11, 8, 1, v4, k4);
        T("custo diferente muda o resultado", memcmp(v, v4, 32) != 0);
    }
    pin_set("123456", salt);
    T("chave do estado existe depois do pin_set", pin_state_key() != NULL);
    { const uint8_t *k = pin_state_key(); T("chave do estado difere do verificador gravado", k && memcmp(k, PIN.hash, 32) != 0); }
    pin_forget_key(); T("pin_forget_key apaga a chave", pin_state_key() == NULL);
    T("pin_check certo devolve a chave", pin_check("123456", 1000) == 0 && pin_state_key() != NULL);
    pin_forget_key();
    T("pin_check errado nao devolve a chave", pin_check("654321", 1001) == 1 && pin_state_key() == NULL);

    /* ---------- passphrases: no truncation, limits / sem truncar, com limites ---------- */
    {
        const char *p40 = "abcdefghijklmnopqrstuvwxyz0123456789ABCD";            /* 40 chars */
        const char *p40b = "abcdefghijklmnopqrstuvwxyz0123456789ABCE";           /* last char differs; the legacy KDF cut at 16 */
        T("senha de 40 caracteres e aceita", pin_set(p40, salt) == 0 && pin_check(p40, 2000) == 0);
        T("senha de 40 caracteres nao e truncada (ultimo caractere diferente falha)", pin_check(p40b, 2001) == 1);
        char p64[65], p65[66]; memset(p64, 'x', 64); p64[64] = 0; memset(p65, 'x', 65); p65[65] = 0;
        T("64 bytes e aceito", pin_set(p64, salt) == 0 && pin_check(p64, 3000) == 0);
        T("65 bytes e recusado no pin_set", pin_set(p65, salt) == -1);
        pin_set("123456", salt);
        T("PIN longo demais no pin_check conta como errado", pin_check(p65, 4000) == 1 && PIN.fails == 1);
        T("PIN vazio e recusado no pin_set", pin_set("", salt) == -1);
        T("recusar o pin_set nao mexe no PIN anterior", pin_check("123456", 5000) == 0);
    }

    /* ---------- v2 state line / linha de estado v2 ---------- */
    {
        char b[256]; pin_set("123456", salt); PIN.fails = 2; PIN.locked_until = 777; pin_serialize(b, sizeof b);
        T("linha v2 comeca com P2 e traz o custo", !strncmp(b, "P2 ", 3) && strstr(b, " 10 8 1\n") != NULL);
        Pin keep = PIN; memset(&PIN, 0, sizeof PIN);
        T("deserializa v2 sem perder nada", pin_deserialize(b) && !memcmp(PIN.salt, keep.salt, 16) && !memcmp(PIN.hash, keep.hash, 32) && PIN.fails == 2 && PIN.locked_until == 777 && PIN.ver == 2);
        T("deserializar nao deixa a chave do estado na memoria", pin_state_key() == NULL);
        T("recusa custo absurdo guardado no estado (N=2^30)", !pin_deserialize("P2 1 0 0 0102030405060708090a0b0c0d0e0f10 0000000000000000000000000000000000000000000000000000000000000000 30 8 1"));
        T("recusa custo que pede memoria demais (r=32, N=2^24)", !pin_deserialize("P2 1 0 0 0102030405060708090a0b0c0d0e0f10 0000000000000000000000000000000000000000000000000000000000000000 24 32 1"));
        T("recusa hash curto", !pin_deserialize("P2 1 0 0 0102030405060708090a0b0c0d0e0f10 00ff 10 8 1"));
        T("recusa linha cortada", !pin_deserialize("P2 1 0 0"));
    }

    /* ---------- no PIN set yet / PIN ainda não definido ----------
     * Found by the fuzzer: an unset PIN was written as "P2 ... 0 0 0" and then refused on load.
     * Achado pelo fuzzer: um PIN não definido era gravado como "P2 ... 0 0 0" e depois recusado ao carregar. */
    {
        memset(&PIN, 0, sizeof PIN);
        char b[256]; pin_serialize(b, sizeof b);
        T("PIN nao definido grava a linha antiga (P, nao P2)", !strncmp(b, "P 0 ", 4));
        memset(&PIN, 0xff, sizeof PIN);
        T("PIN nao definido e lido de volta", pin_deserialize(b) && PIN.set == 0 && PIN.ver == 0);
        T("linha P2 com set=0 tambem e aceita (nao exige custo valido)", pin_deserialize("P2 0 0 0 0102030405060708090a0b0c0d0e0f10 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f 0 0 0") && PIN.set == 0 && PIN.ver == 0);
        T("linha P2 com set=1 e custo zero continua recusada", !pin_deserialize("P2 1 0 0 0102030405060708090a0b0c0d0e0f10 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f 0 0 0"));
    }

    /* ---------- migration from the legacy verifier / migração do verificador antigo ---------- */
    {
        memset(&PIN, 0, sizeof PIN);
        uint8_t ls[16]; for (int i = 0; i < 16; i++) ls[i] = (uint8_t)(0xa0 + i);
        PIN.ver = 1; PIN.set = 1; memcpy(PIN.salt, ls, 16); pin_derive("246810", ls, PIN.hash);
        uint8_t old_hash[32]; memcpy(old_hash, PIN.hash, 32);
        char legacy[200]; pin_serialize(legacy, sizeof legacy);
        T("estado antigo continua gravado no formato antigo (P, 5 campos)", !strncmp(legacy, "P 1 0 0 ", 8) && !strstr(legacy, "P2"));
        memset(&PIN, 0, sizeof PIN);
        T("le o estado antigo como versao 1", pin_deserialize(legacy) && PIN.ver == 1);
        T("PIN errado no estado antigo falha e nao migra", pin_check("000000", 10) == 1 && PIN.ver == 1 && PIN.fails == 1 && pin_state_key() == NULL);
        T("PIN certo no estado antigo abre", pin_check("246810", 11) == 0);
        T("depois do desbloqueio o verificador virou v2", PIN.ver == 2 && PIN.log2n == 10 && PIN.r == 8 && PIN.p == 1 && PIN.fails == 0);
        T("a migracao deixa a chave do estado disponivel", pin_state_key() != NULL);
        char now2[200]; pin_serialize(now2, sizeof now2);
        T("o que sera gravado agora e a linha P2", !strncmp(now2, "P2 ", 3));
        T("o PIN antigo continua valendo depois de migrar", pin_check("246810", 12) == 0);
        T("PIN errado depois de migrar falha", pin_check("246811", 13) == 1);
        T("o verificador gravado agora nao e mais o hash antigo", memcmp(PIN.hash, old_hash, 32) != 0);
    }

    /* ---------- a KDF failure is not a wrong try / falha do KDF não é tentativa errada ---------- */
    {
        pin_set("123456", salt); PIN.log2n = 40;                    /* corrupted in memory: invalid cost */
        int before = PIN.fails;
        T("custo invalido em memoria devolve 3 e nao conta como erro", pin_check("123456", 9000) == 3 && PIN.fails == before);
    }

    /* ---------- the default cost, once / o custo padrão, uma vez ---------- */
    pin_kdf_cost(PIN_DEFAULT_LOG2N, PIN_DEFAULT_R, PIN_DEFAULT_P);
    T("com o custo padrao o PIN funciona", pin_set("123456", salt) == 0 && PIN.log2n == PIN_DEFAULT_LOG2N && pin_check("123456", 20000) == 0 && pin_check("123457", 20001) == 1);

    puts(bad ? "FALHAS" : "TODOS OK"); return bad;
}
