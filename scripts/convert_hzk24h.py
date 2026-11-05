import os

SRC = "HZK24H"
DST = "font24_unicode.bin"
CJK_START = 0x4E00
CJK_END = 0x9FFF
GLYPH_BYTES = 72

def gb2312_to_hzk24h_offset(gb_bytes):
    if len(gb_bytes) != 2:
        return None
    high, low = gb_bytes[0], gb_bytes[1]
    # GB2312 汉字范围检查
    if not (0xB0 <= high <= 0xF7) or not (0xA1 <= low <= 0xFE):
        return None
    area = high - 0xA0
    pos = low - 0xA0
    if area < 16 or pos < 1 or pos > 94:
        return None
    # 公式 A：从第 16 区开始
    index = (area - 1) * 94 + (pos - 1)
    return index * GLYPH_BYTES

def transpose_glyph(col_data):
    """HZK24 按列存储 -> 按行存储"""
    row_data = bytearray(72)
    for row in range(24):
        for col_byte in range(3):
            byte_val = 0
            for bit in range(8):
                col_index = 23 - (col_byte * 8 + bit)
                if col_index >= 24:
                    continue
                src_byte = col_data[col_index * 3 + (row // 8)]
                src_bit = 7 - (row % 8)
                pixel = (src_byte >> src_bit) & 1
                byte_val |= pixel << (7 - bit)
            row_data[row * 3 + col_byte] = byte_val
    return row_data

# 读取源文件
with open(SRC, "rb") as f:
    src = f.read()

print(f"Source HZK24H size: {len(src)} bytes")

# 计算新文件大小
max_code = CJK_END
new_size = (max_code + 1) * GLYPH_BYTES
dst = bytearray(new_size)  # 全零初始化

count = 0
for code in range(CJK_START, CJK_END + 1):
    ch = chr(code)
    try:
        gb = ch.encode("gb2312")
    except UnicodeEncodeError:
        continue  # 该 Unicode 字符不在 GB2312 中

    off = gb2312_to_hzk24h_offset(gb)
    if off is None or off + 72 > len(src):
        continue

    col_data = src[off : off + 72]
    row_data = col_data
    dst[code * GLYPH_BYTES : (code + 1) * GLYPH_BYTES] = row_data
    count += 1

with open(DST, "wb") as f:
    f.write(dst)

print(f"Converted {count} glyphs")
print(f"Output: {DST} ({len(dst)} bytes)")
