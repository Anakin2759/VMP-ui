"""
Fix mojibake in modified files.
Original UTF-8 Chinese bytes were misread as CP936 and re-saved as UTF-8.
Reverse: encode current Unicode as CP936 -> decode bytes as UTF-8.
"""
import pathlib, re, subprocess

GARBLED = re.compile(r'[\u6d5c\u5b29\u6b22\u7039\u6c2b\u7b9f\u8339\u9119\u5c01\u6d4f\u89c8\u5e38'
                     r'\u8499\u6c49\u5b57\u9152\u65b0\u7eb3\u5982\u6c5f\u8bed\u4e49\u5b9a]')

FILES = [
    'src/CMakeLists.txt',
    'src/singleton/Registry.hpp',
    'src/systems/ActionSystem.hpp',
    'src/systems/InteractionSystem.hpp',
    'src/systems/RenderSystem.hpp',
    'src/systems/TweenSystem.hpp',
]

def fix_mojibake(text: str) -> str:
    """Only fix lines that contain mojibake patterns."""
    lines = text.splitlines(keepends=True)
    result = []
    for line in lines:
        if GARBLED.search(line):
            try:
                fixed_line = line.encode('cp936').decode('utf-8')
                result.append(fixed_line)
            except Exception:
                result.append(line)  # leave as-is if can't fix
        else:
            result.append(line)
    return ''.join(result)

for rel in FILES:
    p = pathlib.Path('d:/test/VMP-ui') / rel
    try:
        text = p.read_text(encoding='utf-8')
        fixed = fix_mojibake(text)
        if fixed != text:
            p.write_text(fixed, encoding='utf-8')
            print(f'Fixed: {rel}')
        else:
            print(f'No change needed: {rel}')
    except Exception as e:
        print(f'Error on {rel}: {e}')

print('Done')
