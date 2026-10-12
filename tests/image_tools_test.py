#!/usr/bin/env python3
"""Tests of tools/mkboot.py and tools/mkramdisk.py, the tools that build the image written to the phone. Nothing here touches a device.
Testes do tools/mkboot.py e do tools/mkramdisk.py, as ferramentas que montam a imagem gravada no aparelho. Nada aqui toca um aparelho.
Each PASS/FAIL line is one check. / Cada linha PASS/FAIL é uma verificação."""
import gzip, hashlib, importlib.util, json, os, struct, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
spec = importlib.util.spec_from_file_location("mkboot", os.path.join(ROOT, "tools", "mkboot.py"))
mkboot = importlib.util.module_from_spec(spec); spec.loader.exec_module(mkboot)
fails = 0
def check(name, cond):
    global fails
    print(("PASS " if cond else "FAIL ") + name)
    if not cond: fails += 1

PAGE = 2048
def make_image(path, kernel=b"K" * 5000, ramdisk=b"R" * 3000, second=b"", dtbo=b"D" * 700, dtb=b"T" * 1500, cmdline=b"console=ttyS0 androidboot.x=1"):
    """a synthetic Android boot image v2 / uma imagem de boot Android v2 sintética"""
    ident = hashlib.sha1(kernel + ramdisk).digest().ljust(32, b"\0")
    hdr = struct.pack(mkboot.HDR, b"ANDROID!", len(kernel), 0x8000, len(ramdisk), 0x1000000, len(second), 0, 0x100, PAGE, 2, 0x12345,
                      b"TESTBOOT", cmdline[:512], ident, cmdline[512:], len(dtbo), 0xABCDE, 1660, len(dtb), 0x7FF00000)
    out = hdr.ljust(PAGE, b"\0")
    for b in (kernel, ramdisk, second, dtbo): out += b.ljust(mkboot.pad(len(b), PAGE), b"\0")
    out += dtb.ljust(mkboot.pad(len(dtb), PAGE), b"\0")
    open(path, "wb").write(out)
    return out

def run_pack(src, img, **kw):
    try: mkboot.pack(src, img, **kw); return None
    except AssertionError as e: return str(e)

with tempfile.TemporaryDirectory() as T:
    P = lambda *a: os.path.join(T, *a)
    # ---- the header layout is the one the format defines / o layout do cabeçalho é o do formato ----
    check("o HDR tem 1660 bytes (o tamanho de cabeçalho v2 que o pack grava)", struct.calcsize(mkboot.HDR) == 1660)
    check("pad arredonda para cima ao tamanho da pagina", mkboot.pad(1, 2048) == 2048 and mkboot.pad(2048, 2048) == 2048 and mkboot.pad(2049, 2048) == 4096 and mkboot.pad(0, 2048) == 0)

    # ---- unpack: each part comes out exact, and the header is kept / cada parte sai exata e o cabeçalho é guardado ----
    img = make_image(P("a.img"))
    mkboot.unpack(P("a.img"), P("a"))
    check("unpack: kernel exato", open(P("a", "kernel"), "rb").read() == b"K" * 5000)
    check("unpack: ramdisk exato", open(P("a", "ramdisk"), "rb").read() == b"R" * 3000)
    check("unpack: dtbo e dtb exatos", open(P("a", "dtbo"), "rb").read() == b"D" * 700 and open(P("a", "dtb"), "rb").read() == b"T" * 1500)
    check("unpack: second vazio", open(P("a", "second"), "rb").read() == b"")
    h = json.load(open(P("a", "header.json")))
    check("unpack: enderecos, pagina, versao do SO e nome guardados", h["kernel_addr"] == 0x8000 and h["ramdisk_addr"] == 0x1000000 and h["page"] == PAGE and h["os_version"] == 0x12345 and h["name"] == "TESTBOOT")
    check("unpack: cmdline guardada", h["cmdline"] == "console=ttyS0 androidboot.x=1" and h["dtb_addr"] == 0x7FF00000 and h["dtbo_offset"] == 0xABCDE)

    # ---- round trip: pack(unpack(x)) gives back the same image, except the id which is recomputed / o round trip devolve a mesma imagem ----
    check("pack sem mudar nada: sem erro", run_pack(P("a"), P("b.img")) is None)
    b = open(P("b.img"), "rb").read()
    ID = slice(8 + 40 + 16 + 512, 8 + 40 + 16 + 512 + 32)           # the 32-byte id field / o campo id de 32 bytes
    check("round trip: mesmo tamanho", len(b) == len(img))
    check("round trip: identico fora do campo id", b[:ID.start] + b[ID.stop:] == img[:ID.start] + img[ID.stop:])
    check("round trip: o id e recalculado (sha1 das partes) e preenchido com zeros", b[ID][:20] == hashlib.sha1(
        b"K" * 5000 + struct.pack("<I", 5000) + b"R" * 3000 + struct.pack("<I", 3000) + b"" + struct.pack("<I", 0) + struct.pack("<I", 700) + b"D" * 700 + b"T" * 1500).digest() and b[ID][20:] == b"\0" * 12)
    mkboot.unpack(P("b.img"), P("b"))
    check("depois do round trip, desempacotar de novo da as mesmas partes", all(open(P("a", n), "rb").read() == open(P("b", n), "rb").read() for n in ("kernel", "ramdisk", "dtbo", "dtb", "second")))

    # ---- replacing the ramdisk and the cmdline (what scripts/pack-test-image.sh does) / trocar ramdisk e cmdline ----
    open(P("new.rd"), "wb").write(b"N" * 100)
    check("pack com outro ramdisk e outra cmdline: sem erro", run_pack(P("a"), P("c.img"), ramdisk=P("new.rd"), cmdline="console=ttyS0 bankphone.ro=1 bankphone.devcmd=1") is None)
    mkboot.unpack(P("c.img"), P("c"))
    hc = json.load(open(P("c", "header.json")))
    check("o ramdisk novo entrou e o kernel segue igual", open(P("c", "ramdisk"), "rb").read() == b"N" * 100 and open(P("c", "kernel"), "rb").read() == b"K" * 5000)
    check("a cmdline nova entrou com a trava bankphone.ro=1", "bankphone.ro=1" in hc["cmdline"].split() and "bankphone.devcmd=1" in hc["cmdline"].split())

    # ---- the cmdline boundaries: 512 bytes in the first field, 1024 in the extra one, 1536 refused / bordas da cmdline ----
    def roundtrip_cmd(cmd):
        e = run_pack(P("a"), P("d.img"), cmdline=cmd)
        if e: return e
        mkboot.unpack(P("d.img"), P("d")); return json.load(open(P("d", "header.json")))["cmdline"]
    for n in (511, 512, 513, 1000, 1535):
        cmd = "x" * (n - 15) + " bankphone.ro=1"
        check("cmdline de %d bytes volta inteira (a trava no fim, mesmo cruzando o limite de 512)" % n, len(cmd) == n and roundtrip_cmd(cmd) == cmd)
    check("cmdline de 1536 bytes e recusada (nao cortada em silencio)", run_pack(P("a"), P("e.img"), cmdline="y" * 1536) == "cmdline grande demais" and not os.path.exists(P("e.img")))
    check("cmdline de 5000 bytes e recusada", run_pack(P("a"), P("e.img"), cmdline="y" * 5000) == "cmdline grande demais")
    check("cmdline vazia e aceita e volta vazia", roundtrip_cmd("") == "")

    # ---- what unpack must refuse / o que o unpack tem de recusar ----
    open(P("bad1.img"), "wb").write(b"NOTANIMAGE" + b"\0" * 4000)
    try: mkboot.unpack(P("bad1.img"), P("bad1")); r = "aceitou"
    except AssertionError as e: r = str(e)
    check("unpack recusa arquivo que nao e imagem de boot", "imagem de boot" in r)
    d = bytearray(img); struct.pack_into("<I", d, 8 + 32, 3)         # header version 3 (the 9th u32)  / versao 3
    open(P("v3.img"), "wb").write(bytes(d))
    try: mkboot.unpack(P("v3.img"), P("v3")); r = "aceitou"
    except AssertionError as e: r = str(e)
    check("unpack recusa header de versao diferente de 2", "v2" in r)
    open(P("empty.img"), "wb").write(b"")
    try: mkboot.unpack(P("empty.img"), P("empty")); r = "aceitou"
    except Exception as e: r = type(e).__name__
    check("unpack de arquivo vazio falha (nao produz saida)", r != "aceitou")

    # ---- mkramdisk: a cpio newc archive, gzip, with /init and the assets ----
    os.makedirs(P("assets")); open(P("assets", "b.txt"), "wb").write(b"bbb"); open(P("assets", "a.txt"), "wb").write(b"a" * 7)
    open(P("init"), "wb").write(b"\x7fELF-init")
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "mkramdisk.py"), P("init"), P("rd.gz"), P("assets")], capture_output=True, text=True)
    check("mkramdisk: roda sem erro", r.returncode == 0)
    raw = gzip.decompress(open(P("rd.gz"), "rb").read())
    ents = []; o = 0
    while True:
        assert raw[o:o + 6] == b"070701", "cpio magic"
        f = [int(raw[o + 6 + 8 * i:o + 14 + 8 * i], 16) for i in range(13)]
        mode, filesize, namesize = f[1], f[6], f[11]
        name = raw[o + 110:o + 110 + namesize - 1].decode()
        o += 110 + namesize; o += -o % 4
        data = raw[o:o + filesize]; o += filesize; o += -o % 4
        ents.append((name, mode, data))
        if name == "TRAILER!!!": break
    names = [e[0] for e in ents]
    check("mkramdisk: o arquivo termina em TRAILER!!! e nao sobra lixo", names[-1] == "TRAILER!!!" and o == len(raw))
    check("mkramdisk: tem /init executavel com o conteudo certo", ("init", 0o100755, b"\x7fELF-init") in ents)
    check("mkramdisk: os assets entram em ordem alfabetica, modo 644", ("assets/a.txt", 0o100644, b"a" * 7) in ents and ("assets/b.txt", 0o100644, b"bbb") in ents and names.index("assets/a.txt") < names.index("assets/b.txt"))
    check("mkramdisk: dev/console e dev/null sao nos de dispositivo", any(e[0] == "dev/console" and e[1] == 0o020600 for e in ents) and any(e[0] == "dev/null" and e[1] == 0o020666 for e in ents))
    check("mkramdisk: os diretorios base existem antes do /init", all(d in names for d in ("dev", "proc", "sys", "config", "assets", "tmp")) and names.index("dev") < names.index("init"))
    r2 = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "mkramdisk.py"), P("init"), P("rd2.gz"), P("assets")], capture_output=True)
    check("mkramdisk e reproduzivel: duas execucoes dao os mesmos bytes (mtime=0)", open(P("rd.gz"), "rb").read() == open(P("rd2.gz"), "rb").read())
    os.makedirs(P("extra", "vendor", "firmware")); open(P("extra", "vendor", "firmware", "fw.bin"), "wb").write(b"FW")
    subprocess.run([sys.executable, os.path.join(ROOT, "tools", "mkramdisk.py"), P("init"), P("rd3.gz"), P("assets"), P("extra")], check=True)
    raw3 = gzip.decompress(open(P("rd3.gz"), "rb").read())
    check("mkramdisk: a arvore extra entra (diretorios antes dos arquivos)", b"vendor/firmware/fw.bin" in raw3 and raw3.index(b"vendor\0") < raw3.index(b"vendor/firmware/fw.bin"))

print("TODOS OK" if not fails else "FALHAS")
sys.exit(1 if fails else 0)
