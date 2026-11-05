from fontTools import subset

# --- Configuration ---
SOURCE_FONT = "NotoSansTC-Regular.ttf"
OUTPUT_FONT = "NotoSansTC-Subset-Final.ttf"
TEXT_FILE = "characters.txt"

# --- Read and parse characters from file ---
# Each line looks like "U+4E00" — we need the actual character.
chars = []

with open(TEXT_FILE, 'r', encoding='utf-8') as f:
    for line in f:
        line = line.strip()
        if not line:
            continue
        if line.startswith('U+') or line.startswith('u+'):
            # Parse hex codepoint after "U+"
            try:
                cp = int(line[2:], 16)
                chars.append(chr(cp))
            except ValueError:
                print(f"Skipping malformed line: {line}")
        else:
            # Assume the line is already literal characters
            chars.extend(line)

characters_from_file = ''.join(chars)
print(f"Parsed {len(chars)} characters from {TEXT_FILE}")

# --- Define extra Unicode ranges as actual characters ---
extra_chars = ""

# ASCII: U+0020-007E
extra_chars += ''.join(chr(i) for i in range(0x0020, 0x007F))

# Non-breaking space: U+00A0
extra_chars += chr(0x00A0)

# General Punctuation: U+2010-2027
extra_chars += ''.join(chr(i) for i in range(0x2010, 0x2028))

# Additional Punctuation: U+2030-205E
extra_chars += ''.join(chr(i) for i in range(0x2030, 0x205F))

# CJK Symbols and Punctuation: U+3000-303F
extra_chars += ''.join(chr(i) for i in range(0x3000, 0x3040))

# Vertical Forms: U+FE10-FE4F
extra_chars += ''.join(chr(i) for i in range(0xFE10, 0xFE50))

# Halfwidth and Fullwidth Forms: U+FF00-FFEF
extra_chars += ''.join(chr(i) for i in range(0xFF00, 0xFFF0))

# --- Combine all characters ---
all_text = characters_from_file + extra_chars
print(f"Total characters to subset: {len(set(all_text))}")

# --- Configure the subsetter ---
options = subset.Options()
options.layout_features = ['kern', 'liga']

# --- Load the font ---
font = subset.load_font(SOURCE_FONT, options)

# --- Create the subsetter and populate it ---
subsetter = subset.Subsetter(options=options)
subsetter.populate(text=all_text)

# --- Subset the font and save it ---
subsetter.subset(font)
subset.save_font(font, OUTPUT_FONT, options)

print(f"Subset font saved to {OUTPUT_FONT}")
