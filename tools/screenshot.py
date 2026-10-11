#!/usr/bin/env python3
"""Capture the screen of a phone running a BANKPHONE test image, over the USB cable.

The system sends the frame its UI drew (the buffer in memory, not the physical panel), so it
works even when the screen of the phone is broken. It only works on test images (kernel command
line bankphone.devcmd=1): a production image accepts no commands.

Usage:
  tools/screenshot.py OUT.png                      capture the current screen
  tools/screenshot.py OUT.png --tap 360,1500       tap (x,y) first, then capture
  tools/screenshot.py OUT.png --tap 100,200 --tap 300,400 --wait 0.6
  tools/screenshot.py OUT.png --port /dev/cu.usbmodemBANKPHONE011

---- Português ----
Captura a tela de um celular que roda uma imagem de teste do BANKPHONE, pelo cabo USB.
O sistema manda o quadro que a UI desenhou (o buffer na memória, não o painel físico), então
funciona mesmo com a tela do celular quebrada. Só funciona em imagem de teste (cmdline
bankphone.devcmd=1): uma imagem de produção não aceita comandos.
"""
import argparse, glob, os, select, struct, sys, termios, time, zlib


def find_port():
    for pat in ("/dev/cu.usbmodemBANKPHONE*", "/dev/serial/by-id/*BANKPHONE*"):
        found = sorted(glob.glob(pat))
        if found:
            return found[0]
    return None


def open_port(path):
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    a = termios.tcgetattr(fd)
    a[0] = a[1] = a[3] = 0
    a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    a[4] = a[5] = termios.B115200
    termios.tcsetattr(fd, termios.TCSANOW, a)
    return fd


def send(fd, line):
    os.write(fd, ("\n" + line + "\n").encode())


def read_until(fd, buf, marker, deadline):
    """Reads from the port until `marker` is in `buf`. Returns the new buf."""
    while marker not in buf:
        left = deadline - time.time()
        if left <= 0:
            raise TimeoutError("timeout waiting for the phone")
        if select.select([fd], [], [], min(left, 0.5))[0]:
            try:
                chunk = os.read(fd, 65536)
            except (BlockingIOError, InterruptedError):
                continue
            if not chunk:
                raise ConnectionError("the port closed")
            buf += chunk
    return buf


def write_png(path, w, h, rgb):
    raw = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def capture(fd, timeout=90):
    deadline = time.time() + timeout
    send(fd, "BANKPHONE:SCREENSHOT")
    buf = read_until(fd, b"", b"BANKPHONE:FRAME ", deadline)
    buf = buf[buf.index(b"BANKPHONE:FRAME "):]
    buf = read_until(fd, buf, b"\n", deadline)
    head, _, rest = buf.partition(b"\n")
    parts = head.split()
    w, h, n = int(parts[1]), int(parts[2]), int(parts[3])
    rle = len(parts) > 4 and parts[4] == b"rle"
    if w <= 0 or h <= 0 or n <= 0:
        raise ValueError("the phone has no frame to send (%dx%d, %d bytes)" % (w, h, n))
    rest = bytearray(rest)
    while len(rest) < n + 15:                          # data + the END marker / dados + a marca de fim
        left = deadline - time.time()
        if left <= 0:
            raise TimeoutError("timeout receiving the frame (%d of %d bytes)" % (len(rest), n))
        if select.select([fd], [], [], min(left, 0.5))[0]:
            try:
                rest += os.read(fd, 1 << 20)
            except (BlockingIOError, InterruptedError):
                pass
    if bytes(rest[n:n + 15]).strip() != b"BANKPHONE:END":
        raise ValueError("the frame is corrupted or truncated (no END marker after %d bytes)" % n)
    data = bytes(rest[:n])
    if not rle:
        if n != w * h * 3:
            raise ValueError("raw frame with the wrong size")
        return w, h, data
    if n % 4:
        raise ValueError("RLE frame with a size that is not a multiple of 4")
    out = bytearray()
    for i in range(0, n, 4):
        cnt = data[i]
        if cnt == 0:
            raise ValueError("RLE frame with a zero-length run")
        out += data[i + 1:i + 4] * cnt
    if len(out) != w * h * 3:
        raise ValueError("RLE frame decodes to %d bytes, expected %d" % (len(out), w * h * 3))
    return w, h, bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", help="output PNG file")
    ap.add_argument("--tap", action="append", default=[], metavar="X,Y", help="tap before the capture (repeatable)")
    ap.add_argument("--wait", type=float, default=0.5, help="seconds to wait after each tap (default 0.5)")
    ap.add_argument("--port", help="serial port (default: auto-detect)")
    args = ap.parse_args()

    port = args.port or find_port()
    if not port:
        sys.exit("ERROR: no BANKPHONE serial port found. Is the phone plugged in and running a test image?")
    fd = open_port(port)
    try:
        while select.select([fd], [], [], 0.2)[0]:       # drain old log lines / descarta o log antigo
            try:
                os.read(fd, 1 << 20)
            except (BlockingIOError, InterruptedError):
                break
        for t in args.tap:
            try:
                x, y = (int(v) for v in t.split(","))
            except ValueError:
                sys.exit("ERROR: --tap needs X,Y (for example --tap 360,1500)")
            send(fd, "BANKPHONE:TOUCH 0 %d %d" % (x, y))
            time.sleep(0.12)
            send(fd, "BANKPHONE:TOUCH 2 %d %d" % (x, y))
            time.sleep(args.wait)
        w, h, rgb = capture(fd)
    except (TimeoutError, ConnectionError, ValueError, OSError) as e:
        sys.exit("ERROR: %s" % e)
    finally:
        os.close(fd)
    write_png(args.out, w, h, rgb)
    print("saved %s (%dx%d)" % (args.out, w, h))


if __name__ == "__main__":
    main()
