#!/usr/bin/env python3
"""BANKPHONE: monta initramfs (cpio newc + gzip) com /init e /assets. Escrito do zero."""
import gzip, os, sys

def ent(name, mode, data=b"", ino=[1], rdev=(0, 0)):
    ino[0] += 1
    nm = name.encode() + b"\0"
    h = b"070701" + b"".join(b"%08X" % v for v in (ino[0], mode, 0, 0, 1, 0, len(data), 0, 0, rdev[0], rdev[1], len(nm), 0))
    out = h + nm; out += b"\0" * (-len(out) % 4); out += data; out += b"\0" * (-len(data) % 4); return out

init, outp, assets = sys.argv[1], sys.argv[2], sys.argv[3]
extra = sys.argv[4] if len(sys.argv) > 4 else None
b = b""
for d in ("dev", "proc", "sys", "config", "assets", "tmp"): b += ent(d, 0o040755)
b += ent("dev/console", 0o020600, rdev=(5, 1)) + ent("dev/null", 0o020666, rdev=(1, 3))
b += ent("init", 0o100755, open(init, "rb").read())
for f in sorted(os.listdir(assets)): b += ent("assets/" + f, 0o100644, open(f"{assets}/{f}", "rb").read())
if extra:   # árvore extra (ex.: vendor/firmware/ com o firmware do toque), diretórios antes dos arquivos
    for root, dirs, files in os.walk(extra):
        dirs.sort(); rel = os.path.relpath(root, extra)
        if rel != ".": b += ent(rel, 0o040755)
        for f in sorted(files): b += ent(os.path.normpath(os.path.join(rel, f)), 0o100644, open(os.path.join(root, f), "rb").read())
b += ent("TRAILER!!!", 0)
open(outp, "wb").write(gzip.compress(b, 9, mtime=0))
