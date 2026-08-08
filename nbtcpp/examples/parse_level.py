import gzip, struct

with gzip.open("K:/nbtcpp/level.dat", "rb") as f:
    data = f.read()
print(f"Decompressed: {len(data)} bytes")

# Parse NBT root level
pos = 0
children = []
while pos < len(data):
    t = data[pos]
    if t == 0:
        print(f"  TAG_End at pos {pos}")
        break
    nl = struct.unpack(">H", data[pos+1:pos+3])[0]
    nm = data[pos+3:pos+3+nl].decode("utf-8")
    print(f"  type={t} name=\"{nm}\" len={nl} pos={pos}")
    children.append((t, nm, pos))
    pos += 3 + nl
    # If compound, skip past end tag (naive)
    if t == 0x0A:
        depth = 1
        while depth > 0 and pos < len(data):
            if data[pos] == 0:
                depth -= 1
            elif data[pos] == 0x0A:
                depth += 1
            pos += 1

print(f"\nRoot-level children: {len(children)}")
for t, nm, p in children:
    print(f"  \"{nm}\" (type {t}) at offset {p}")
