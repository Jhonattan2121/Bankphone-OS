/* Interaction checks between the scrypt PIN (PR #13) and what main gained after it (N07 grant, PIN lockout bounded to 1920 s).
 * Verificações de interação entre o PIN com scrypt (PR #13) e o que a main ganhou depois (concessão N07, bloqueio do PIN limitado a 1920 s).
 *
 * REFERENCE ONLY, not part of `make test`. Built against the TRIAL merge of main into sec/scrypt-pin-kdf:
 *   cc -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -Iinit \
 *      docs/review/reconcile/pr13_main_interaction_test.c init/sec.c -o /tmp/interact && /tmp/interact
 * SÓ REFERÊNCIA, fora do `make test`. Compilado contra o merge de TESTE da main em sec/scrypt-pin-kdf.
 * 16 checks, all PASS on the trial merge (round 4). It is not evidence about the phone. / 16 verificações, todas PASS no merge de teste. Não é evidência do aparelho. */
#include "sec.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static int bad; static void T(const char*n,int ok){printf("%s %s\n",ok?"PASS":"FAIL",n);if(!ok)bad++;}
static double ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e3+t.tv_nsec/1e6;}
int main(void){
  uint8_t salt[16]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
  /* A. legacy (P) verifier migrates AND still issues the grant */
  memset(&PIN,0,sizeof PIN); PIN.ver=1; PIN.set=1; memcpy(PIN.salt,salt,16); pin_derive("246810",salt,PIN.hash);
  pin_kdf_cost(10,8,1);
  auth_clear(); auth_expect("txA");
  T("A1 legacy PIN right: ok", pin_check("246810",100)==0);
  T("A2 legacy PIN right: migrated to v2", PIN.ver==2);
  T("A3 legacy PIN right: grant issued for the tx", auth_peek("txA",100)==L_PIN);
  /* B. wrong PIN issues nothing, right PIN for another tx does not unlock this one */
  auth_clear(); auth_expect("txB"); pin_check("000000",101);
  T("B1 wrong PIN: no grant", auth_peek("txB",101)==L_NONE);
  T("B2 grant is for txB only", (pin_check("246810",102)==0) && auth_peek("txB",102)==L_PIN && auth_peek("txOther",102)==L_NONE);
  /* C. locked: the KDF must not run (no free CPU/memory burn, no oracle) */
  pin_kdf_cost(PIN_DEFAULT_LOG2N,PIN_DEFAULT_R,PIN_DEFAULT_P); pin_set("123456",salt);
  for(int i=0;i<5;i++) pin_check("000000",1000);
  double t0=ms(); int r=pin_check("123456",1001); double dt=ms()-t0;
  T("C1 locked: right PIN refused (2)", r==2); printf("   locked check took %.2f ms\n",dt);
  T("C2 locked check does not run the KDF (<5 ms at default cost)", dt<5.0);
  T("C3 no grant while locked", (auth_expect("txC"), pin_check("123456",1002)==2 && auth_peek("txC",1002)==L_NONE));
  /* D. state key lifecycle */
  pin_set("123456",salt); pin_forget_key();
  T("D1 key gone after forget", pin_state_key()==NULL);
  T("D2 wrong PIN gives no key", pin_check("999999",5000)==1 && pin_state_key()==NULL);
  pin_forget_key(); PIN.fails=0; PIN.locked_until=0;
  T("D3 right PIN gives the key", pin_check("123456",9000)==0 && pin_state_key()!=NULL);
  /* E. clock domains with a v2 line: wall-clock lock read at uptime */
  char line[300]; pin_set("123456",salt); for(int i=0;i<5;i++) pin_check("000000",1759468800);
  pin_serialize(line,sizeof line); memset(&PIN,0,sizeof PIN); T("E1 P2 line reloads",pin_deserialize(line)==1 && PIN.ver==2);
  T("E2 lock from the other clock is bounded (<=1920 s)", pin_locked_s(5)<=PIN_LOCK_MAX_S && pin_locked_s(5)>0);
  /* F. a counter above the bound is REFUSED at load (not clamped) */
  pin_set("123456",salt); pin_serialize(line,sizeof line);
  { char *p=strstr(line," 0 0 "); (void)p; }
  char bad1[300]; snprintf(bad1,sizeof bad1,"P2 1 5000 0 %s",strchr(strchr(strchr(strchr(line,' ')+1,' ')+1,' ')+1,' ')+1);
  Pin keep=PIN; T("F1 counter 5000 refused at load", pin_deserialize(bad1)==0);
  T("F2 a refused line leaves PIN untouched", PIN.fails==keep.fails && PIN.ver==keep.ver);
  puts(bad?"FALHAS":"TODOS OK"); return bad; }
