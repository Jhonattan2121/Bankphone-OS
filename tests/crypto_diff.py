#!/usr/bin/env python3
"""Generate random test cases for tests/crypto_diff.c using OpenSSL (via hashlib/hmac) as the reference.
Gera casos aleatórios para tests/crypto_diff.c usando o OpenSSL (via hashlib/hmac) como referência.

A fixed seed makes a failure repeat. If this Python has no hashlib.scrypt (some macOS builds), it prints
a note to stderr and emits nothing: the C side then reports 0 cases and the test is skipped, not failed.
Uma semente fixa faz a falha se repetir. Se este Python não tiver hashlib.scrypt (alguns macOS), avisa no
stderr e não emite nada: o lado em C vê 0 casos e o teste é pulado, não reprovado.

Line format / formato da linha:  kind a b x y z dklen expected   ('-' = empty / vazio)
"""
import hashlib, hmac, random, sys

if not hasattr(hashlib, "scrypt"):
    sys.stderr.write("crypto_diff: this Python has no hashlib.scrypt: skipped / sem hashlib.scrypt: pulado\n")
    sys.exit(0)

random.seed(20261011)
rb = lambda n: bytes(random.randrange(256) for _ in range(n))
h = lambda b: b.hex() if b else "-"

for _ in range(300):
    pw = rb(random.choice([0, 1, 2, 5, 16, 31, 32, 33, 63, 64, 65, 100]))
    salt = rb(random.choice([0, 1, 8, 16, 17, 32, 55, 56, 64, 100]))
    log2n, r, p = random.randint(1, 9), random.randint(1, 6), random.randint(1, 4)
    dk = random.choice([1, 16, 31, 32, 33, 64, 65, 100])
    exp = hashlib.scrypt(pw, salt=salt, n=1 << log2n, r=r, p=p, dklen=dk, maxmem=2**27).hex()
    print("scrypt", h(pw), h(salt), log2n, r, p, dk, exp)
for _ in range(200):
    pw = rb(random.choice([0, 1, 20, 64, 65, 128, 200]))
    salt = rb(random.choice([0, 4, 16, 64, 100]))
    c, dk = random.choice([1, 2, 3, 10, 100]), random.choice([1, 32, 33, 64, 100])
    print("pbkdf2", h(pw), h(salt), c, 0, 0, dk, hashlib.pbkdf2_hmac("sha256", pw, salt, c, dk).hex())
for _ in range(200):
    key = rb(random.choice([0, 1, 32, 63, 64, 65, 131]))
    msg = rb(random.choice([0, 1, 55, 56, 63, 64, 65, 119, 120, 128, 300]))
    print("hmac", h(key), h(msg), 0, 0, 0, 32, hmac.new(key, msg, hashlib.sha256).hexdigest())
for _ in range(150):
    ikm = rb(random.choice([1, 22, 32, 80]))
    salt, info = rb(random.choice([0, 13, 32, 100])), rb(random.choice([0, 10, 40]))
    length = random.choice([1, 32, 33, 64, 42, 255])
    prk = hmac.new(salt if salt else b"\0" * 32, ikm, hashlib.sha256).digest()
    t = o = b""; k = 1
    while len(o) < length:
        t = hmac.new(prk, t + info + bytes([k]), hashlib.sha256).digest(); o += t; k += 1
    print("hkdf", h(ikm), h(salt) + ":" + h(info), 0, 0, 0, length, o[:length].hex())
