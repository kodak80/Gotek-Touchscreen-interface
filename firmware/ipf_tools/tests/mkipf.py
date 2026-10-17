#!/usr/bin/env python3
"""Generate a synthetic Amiga IPF (SPS encoder) exercising converter cases.

Tracks (cyl/side):
  0/0  standard AmigaDOS 11 sectors, Auto density, 100000 cells
  0/1  same, data-over-index (startBitPos = 90000)
  1/0  long track: 105000 cells, Auto density
  1/1  Copylock Amiga density (density type 3)
  2/0  weak/fuzzy bits inside sector 5 data
  2/1  odd length track (100003 cells)
  3/0  unformatted (noise)
  3/1  standard (short track 98000 cells)
Sector payload byte i of sector s on track t = (t*31 + s*7 + i) & 0xFF.
"""
import struct, sys, zlib

def be32(*v):
    return struct.pack('>%dI' % len(v), *v)

def record(rtype, body):
    hdr = rtype + be32(12 + len(body), 0)
    crc = zlib.crc32(hdr + body) & 0xFFFFFFFF
    return rtype + be32(12 + len(body), crc) + body

def split_oddeven(data):
    """Amiga odd/even split, returned as packed decoded bytes (odd then even)."""
    odd, even = [], []
    for b in data:
        for bit in (7, 5, 3, 1):
            odd.append((b >> bit) & 1)
        for bit in (6, 4, 2, 0):
            even.append((b >> bit) & 1)
    def pack(bits):
        out = bytearray()
        for i in range(0, len(bits), 8):
            v = 0
            for b in bits[i:i+8]:
                v = (v << 1) | b
            out.append(v)
        return bytes(out)
    return pack(odd) + pack(even)

def xor16(packed):
    x = 0
    for i in range(0, len(packed), 2):
        x ^= (packed[i] << 8) | packed[i+1]
    return x

def payload(track, sec):
    return bytes(((track * 31 + sec * 7 + i) & 0xFF) for i in range(512))

def elem(etype, data=b'', count=None):
    n = len(data) if count is None else count
    w = 1 if n < 256 else 2 if n < 65536 else 3
    return bytes([(w << 5) | etype]) + n.to_bytes(w, 'big') + data

SYNC, DATA, GAP, FUZZY = 1, 2, 3, 5

def amiga_sector_stream(track, sec, fuzzy=False):
    """Data stream elements for one Amiga sector; returns (bytes, cells)."""
    info = bytes([0xFF, track, sec, 11 - sec])
    label = bytes(16)
    info_p = split_oddeven(info)
    label_p = split_oddeven(label)
    hck = xor16(info_p + label_p)
    data = payload(track, sec)
    data_p = split_oddeven(data)
    dck = xor16(data_p)
    hck_p = bytes([0, 0, hck >> 8, hck & 0xFF])
    dck_p = bytes([0, 0, dck >> 8, dck & 0xFF])
    s = elem(DATA, b'\x00\x00')
    s += elem(SYNC, b'\x44\x89\x44\x89')
    s += elem(DATA, info_p + label_p + hck_p + dck_p)
    if fuzzy:
        s += elem(DATA, data_p[:200])
        s += elem(FUZZY, count=64)
        s += elem(DATA, data_p[264:])
    else:
        s += elem(DATA, data_p)
    s += b'\x00'
    cells = 2 * 16 + 32 + (4 + 16 + 4 + 4 + 512) * 16
    return s, cells

def make_track(cyl, side, track_cells, start_bit=0, density=2, fuzzy_sec=None,
               noise=False):
    t = cyl * 2 + side
    if noise:
        return dict(cyl=cyl, side=side, density=1, blocks=[], streams=b'',
                    start=0, cells=0, fuzzy=False)
    blocks, streams = [], b''
    for sec in range(11):
        s, cells = amiga_sector_stream(t, sec, fuzzy=(sec == fuzzy_sec))
        blocks.append([cells, 0, len(streams)])
        streams += s
    used = sum(b[0] for b in blocks)
    blocks[-1][1] = track_cells - used    # all slack in the track gap
    assert blocks[-1][1] > 0
    return dict(cyl=cyl, side=side, density=density, blocks=blocks,
                streams=streams, start=start_bit, cells=track_cells,
                fuzzy=fuzzy_sec is not None)

def build(tracks, ncyl):
    out = record(b'CAPS', b'')
    info = be32(1, 2, 1, 0x1234, 1, 0, 0, ncyl - 1, 0, 1, 20261017, 0,
                1, 0, 0, 0, 0, 0, 0, 0, 0)
    out += record(b'INFO', info)
    imges, datas = b'', b''
    for key, tr in enumerate(tracks, 1):
        nb = len(tr['blocks'])
        dbits = sum(b[0] for b in tr['blocks'])
        gbits = sum(b[1] for b in tr['blocks'])
        imges += record(b'IMGE', be32(
            tr['cyl'], tr['side'], tr['density'], 1,
            (tr['cells'] + 7) // 8, tr['start'] // 8, tr['start'],
            dbits, gbits, dbits + gbits, nb, 0,
            1 if tr['fuzzy'] else 0, key, 0, 0, 0))
        desc = b''
        base = 32 * nb
        for cells, gap, off in tr['blocks']:
            desc += be32(cells, gap, 0, 1, 1, 0, 0, base + off)
        extra = desc + tr['streams']
        crc = zlib.crc32(extra) & 0xFFFFFFFF if extra else 0
        datas += record(b'DATA', be32(len(extra), len(extra) * 8, crc, key)) + extra
    return out + imges + datas

def main():
    tracks = [
        make_track(0, 0, 100000),
        make_track(0, 1, 100000, start_bit=90000),
        make_track(1, 0, 105000),
        make_track(1, 1, 100000, density=3),
        make_track(2, 0, 100000, fuzzy_sec=5),
        make_track(2, 1, 100003),
        make_track(3, 0, 0, noise=True),
        make_track(3, 1, 98000),
    ]
    open(sys.argv[1], 'wb').write(build(tracks, 4))

if __name__ == '__main__':
    main()
