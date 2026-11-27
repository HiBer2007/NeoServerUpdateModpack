import re, pathlib, hashlib

root = pathlib.Path('docs/zh')
out = pathlib.Path('docs/assets/diagrams')
out.mkdir(parents=True, exist_ok=True)
seen = {}
n = 0
for p in sorted(root.rglob('*.md')):
    text = p.read_text(encoding='utf-8')
    for m in re.finditer(r'```mermaid\n(.*?)```', text, re.S):
        code = m.group(1).strip()
        h = hashlib.sha1(code.encode()).hexdigest()
        if h in seen:
            continue
        seen[h] = p
        n += 1
        if n > 15:
            break
        rel = str(p.relative_to(root)).replace('\\', '/').replace('/', '-').replace('.md', '')
        name = f'{n:02d}-{rel}.mmd'
        (out / name).write_text(code + '\n', encoding='utf-8')
        print(name)
    if n > 15:
        break
print('total', min(n, 15))
