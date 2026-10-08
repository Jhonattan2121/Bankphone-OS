#!/usr/bin/env python3
"""BANKPHONE: desmonta e remonta imagem de boot Android v2 (kernel, ramdisk, dtb). Escrito do zero."""
import hashlib, json, os, struct, sys

HDR = "<8s10I16s512s32s1024sIQIIQ"   # magic + 10 u32 + name + cmdline + id + extra + dtbo_size + dtbo_off + hdr_size + dtb_size + dtb_addr

def pad(n, p): return (n + p - 1) // p * p

def unpack(img, out):
    d = open(img, "rb").read()
    f = struct.unpack_from(HDR, d, 0)
    assert f[0] == b"ANDROID!", "não é imagem de boot"
    ks, ka, rs, ra, ss, sa, ta, page, ver, osv = f[1:11]
    assert ver == 2, f"só header v2 (veio v{ver})"
    name, cmd, _id, extra, dtbo_sz, dtbo_off, hsz, dtb_sz, dtb_addr = f[11:]
    os.makedirs(out, exist_ok=True)
    o = page
    parts = {}
    for n, sz in (("kernel", ks), ("ramdisk", rs), ("second", ss)):
        parts[n] = d[o:o + sz]; o += pad(sz, page)
    parts["dtbo"] = d[o:o + dtbo_sz]; o += pad(dtbo_sz, page)
    parts["dtb"] = d[o:o + dtb_sz]
    for n, b in parts.items(): open(f"{out}/{n}", "wb").write(b)
    json.dump({"kernel_addr": ka, "ramdisk_addr": ra, "second_addr": sa, "tags_addr": ta, "page": page, "os_version": osv,
               "name": name.rstrip(b"\0").decode(), "cmdline": (cmd.rstrip(b"\0") + extra.rstrip(b"\0")).decode(),
               "dtb_addr": dtb_addr, "dtbo_offset": dtbo_off}, open(f"{out}/header.json", "w"), indent=1)

def pack(src, img, ramdisk=None, cmdline=None):
    h = json.load(open(f"{src}/header.json")); page = h["page"]
    rd = lambda n: open(f"{src}/{n}", "rb").read()
    kernel, second, dtbo, dtb = rd("kernel"), rd("second"), rd("dtbo"), rd("dtb")
    ram = open(ramdisk, "rb").read() if ramdisk else rd("ramdisk")
    cmd = (cmdline if cmdline is not None else h["cmdline"]).encode()
    assert len(cmd) < 1536, "cmdline grande demais"
    sha = hashlib.sha1()
    for b in (kernel, ram, second):
        sha.update(b); sha.update(struct.pack("<I", len(b)))
    sha.update(struct.pack("<I", len(dtbo))); sha.update(dtbo)
    sha.update(dtb)
    ident = sha.digest().ljust(32, b"\0")
    hdr = struct.pack(HDR, b"ANDROID!", len(kernel), h["kernel_addr"], len(ram), h["ramdisk_addr"], len(second), h["second_addr"],
                      h["tags_addr"], page, 2, h["os_version"], h["name"].encode(), cmd[:512], ident, cmd[512:],
                      len(dtbo), h["dtbo_offset"], 1660, len(dtb), h["dtb_addr"])
    out = hdr.ljust(page, b"\0")
    for b in (kernel, ram, second, dtbo): out += b.ljust(pad(len(b), page), b"\0")
    out += dtb.ljust(pad(len(dtb), page), b"\0")
    open(img, "wb").write(out)

if __name__ == "__main__":
    a = sys.argv
    if a[1] == "unpack": unpack(a[2], a[3])
    elif a[1] == "pack":
        kw = {}
        i = 4
        while i < len(a): kw[a[i].lstrip("-")] = a[i + 1]; i += 2
        pack(a[2], a[3], kw.get("ramdisk"), kw.get("cmdline"))
