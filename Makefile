# BANKPHONE OS: checks that run on a computer (no phone needed).
# BANKPHONE OS: checagens que rodam no computador (sem o celular).
#
#   make test        build and run all tests            / compila e roda todos os testes
#   make test-asan   same, with AddressSanitizer + UBSan / o mesmo com AddressSanitizer + UBSan
#   make lint        -Wall -Wextra -Werror, syntax only  / só checa a sintaxe
#   make lint-strict extra warnings, never fails         / avisos extras, sem falhar
#   make fuzz        run the fuzz targets (FUZZ_TIME=s)  / roda os alvos de fuzzing (FUZZ_TIME=segundos)
#   make clean
#
# The installer and the phone build (install.sh, build.sh) do not go through here.
# O instalador e o build do aparelho (install.sh, build.sh) não passam por aqui.

CC       ?= cc
BUILD    ?= build/plain
CFLAGS   ?= -O1 -g -Wall -Wextra -Wno-unused-function -Wno-unused-parameter
SAN      ?=

ASAN_FLAGS := -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer

# Test name -> sources and flags (same commands as the README).
# Nome do teste -> fontes e flags (os mesmos comandos do README).
TESTS := money sec devcmd store touchcore bootdiag_fb

SRC_money       := tests/money_test.c init/money.c
LIBS_money      := -lm
SRC_sec         := tests/sec_test.c init/sec.c
LIBS_sec        := -lm
SRC_devcmd      := tests/devcmd_test.c
SRC_store       := init/tests/test_store.c init/store.c init/sec.c
FLAGS_store     := -DBANKPHONE_STORE_TEST
LIBS_store      := -lm
SRC_touchcore   := init/tests/test_touchcore.c
SRC_bootdiag_fb := init/tests/test_bootdiag_fb.c init/bootdiag.c
FLAGS_bootdiag_fb := -Iinit

HDRS := $(wildcard init/*.h)
BINS := $(addprefix $(BUILD)/,$(TESTS))

# Sources that build on any computer (main.c and hw.c use the phone's Linux headers).
# Fontes que compilam em qualquer computador (main.c e hw.c usam headers do Linux do aparelho).
LINT_SRC    := init/money.c init/sec.c init/store.c init/gfx.c init/ui.c init/screens.c \
               init/sheets.c init/components.c init/icons.c init/bootdiag.c init/host.c
LINT_DEVICE := init/main.c init/hw.c
# Project style: several 'if' on one line. Not a bug, so it must not fail the lint.
# Estilo do projeto: vários 'if' na mesma linha. Não é bug, então não derruba o lint.
LINT_FLAGS  := -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-parameter \
               -Wno-misleading-indentation -Iinit -fsyntax-only

.SECONDEXPANSION:
.PHONY: all test test-asan lint lint-strict fuzz clean

all: test

$(BUILD)/%: $$(SRC_%) $(HDRS) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) $(FLAGS_$*) -o $@ $(SRC_$*) $(LIBS_$*)

$(BUILD):
	mkdir -p $@

# Each test prints in its own way; here we only count how many checks passed.
# Cada teste imprime do seu jeito; aqui só se conta quantas verificações passaram.
test: $(BINS)
	@fail=0; total=0; \
	for t in $(TESTS); do \
	  log=$(BUILD)/$$t.log; \
	  if $(BUILD)/$$t > $$log 2>&1; then r=ok; else r=FALHOU; fail=1; fi; \
	  case $$t in \
	    money|sec|devcmd) n=$$(grep -c '^PASS' $$log) ;; \
	    store)            n=$$(sed -n 's/.*RESULTADO: \([0-9]*\) verifica.*/\1/p' $$log | tail -1) ;; \
	    touchcore)        n=$$(sed -n 's/^\([0-9]*\) verificacoes.*/\1/p' $$log | tail -1) ;; \
	    bootdiag_fb)      n=$$(grep -c '^  ok' $$log) ;; \
	  esac; \
	  total=$$((total + $${n:-0})); \
	  printf '%-12s %-7s %s verificacoes\n' $$t $$r "$${n:-?}"; \
	  if [ $$r != ok ]; then grep -v 'linha [0-9]' $$log | tail -15; fi; \
	done; \
	echo "total: $$total verificacoes"; \
	if [ $$fail = 0 ]; then echo "TODOS OK"; else echo "HA TESTES FALHANDO"; fi; \
	exit $$fail

test-asan:
	$(MAKE) test BUILD=build/asan SAN="$(ASAN_FLAGS)"

lint:
	$(CC) $(LINT_FLAGS) $(LINT_SRC)
ifeq ($(shell uname -s),Linux)
	$(CC) $(LINT_FLAGS) $(LINT_DEVICE)
endif
	@echo "lint OK"

lint-strict:
	-$(CC) -Wall -Wextra -Wshadow -Wconversion -Wno-unused-function -Wno-unused-parameter \
	  -Wno-misleading-indentation -Iinit -fsyntax-only init/money.c init/sec.c init/store.c

# Fuzzing. Our own driver (tests/fuzz/driver.c) works with any C compiler: it runs the corpus,
# then mutates inputs for FUZZ_TIME seconds. Fixed seed (FUZZ_SEED=n changes it), so a failure
# repeats. With clang you can use libFuzzer too: make fuzz FUZZ_ENGINE=libfuzzer
#
# Fuzzing. O driver próprio (tests/fuzz/driver.c) roda com qualquer compilador C: roda o corpus
# e depois muta entradas por FUZZ_TIME segundos. Semente fixa (FUZZ_SEED=n muda), então uma
# falha se repete. Com clang também dá para usar o libFuzzer: make fuzz FUZZ_ENGINE=libfuzzer
FUZZ_TIME   ?= 30
FUZZ_ENGINE ?= driver
FUZZ_TARGETS := devcmd money
FUZZ_SAN    := -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer
ifeq ($(FUZZ_ENGINE),libfuzzer)
FUZZ_CC     := clang
FUZZ_FLAGS  := -g -O1 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined -Iinit
FUZZ_DRIVER :=
FUZZ_RUN     = build/fuzz/fuzz_$(1) -max_total_time=$(FUZZ_TIME) tests/fuzz/corpus/$(1)
else
FUZZ_CC     := $(CC)
FUZZ_FLAGS  := -g -O1 $(FUZZ_SAN) -Iinit
FUZZ_DRIVER := tests/fuzz/driver.c
FUZZ_RUN     = build/fuzz/fuzz_$(1) $(FUZZ_TIME) tests/fuzz/corpus/$(1)
endif

SRC_fuzz_devcmd := tests/fuzz/fuzz_devcmd.c
SRC_fuzz_money  := tests/fuzz/fuzz_money.c init/money.c
LIBS_fuzz_money := -lm

build/fuzz/fuzz_%: $$(SRC_fuzz_%) $(FUZZ_DRIVER) $(HDRS) tests/fuzz/driver.c
	@mkdir -p build/fuzz
	$(FUZZ_CC) $(FUZZ_FLAGS) -o $@ $(FUZZ_DRIVER) $(SRC_fuzz_$*) $(LIBS_fuzz_$*)

fuzz: $(addprefix build/fuzz/fuzz_,$(FUZZ_TARGETS))
	@for f in $(FUZZ_TARGETS); do \
	  echo "== fuzz_$$f ($(FUZZ_TIME)s, $(FUZZ_ENGINE))"; \
	  case $$f in devcmd) $(call FUZZ_RUN,devcmd) ;; money) $(call FUZZ_RUN,money) ;; esac || exit 1; \
	done

clean:
	rm -rf build
