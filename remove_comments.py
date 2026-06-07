import sys
import os
import glob

def remove_cpp_comments(source: str) -> str:
    result = []
    i = 0
    n = len(source)

    while i < n:
        if source[i] == '"':
            j = i + 1
            while j < n:
                if source[j] == '\\':
                    j += 2
                    continue
                if source[j] == '"':
                    j += 1
                    break
                j += 1
            result.append(source[i:j])
            i = j

        elif source[i] == "'":
            j = i + 1
            while j < n:
                if source[j] == '\\':
                    j += 2
                    continue
                if source[j] == "'":
                    j += 1
                    break
                j += 1
            result.append(source[i:j])
            i = j

        elif source[i:i+2] == '/*':
            j = source.find('*/', i + 2)
            if j == -1:
                break
            i = j + 2

        elif source[i:i+2] == '//':
            j = source.find('\n', i + 2)
            if j == -1:
                break
            i = j

        else:
            result.append(source[i])
            i += 1

    return ''.join(result)


def clean_blank_lines(source: str) -> str:
    lines = source.split('\n')
    cleaned = []
    blank_count = 0
    for line in lines:
        stripped = line.rstrip()
        if stripped == '':
            blank_count += 1
            if blank_count <= 1:
                cleaned.append('')
        else:
            blank_count = 0
            cleaned.append(stripped)
    return '\n'.join(cleaned).strip() + '\n'


def process_file(path: str) -> None:
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        original = f.read()

    result = clean_blank_lines(remove_cpp_comments(original))

    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(result)

    print(f'[done] {path}')


def main():
    targets = sys.argv[1:] if len(sys.argv) > 1 else ['src']
    extensions = ('.h', '.cpp')
    files = []

    for target in targets:
        if os.path.isfile(target):
            files.append(target)
        elif os.path.isdir(target):
            for ext in extensions:
                pattern = os.path.join(target, '**', f'*{ext}')
                files.extend(glob.glob(pattern, recursive=True))
        else:
            files.extend(glob.glob(target, recursive=True))

    if not files:
        print('没有找到任何文件。')
        return

    for path in sorted(set(files)):
        process_file(path)

    print(f'\n完成，共处理 {len(files)} 个文件。')


if __name__ == '__main__':
    main()