#!/usr/bin/env python3
"""用 `chapter all` 的输出刷新各章 README 里「在镜像里复现」的回显。

只替换回显代码块。知识点、架构和成功判据是手写的，这个脚本不会碰。
时间戳、地址和数字不同不算变化，只有命令或回显的结构变了才会改写。

    docker run --rm ldd-lab chapter all > /tmp/all.log
    python3 qemu_test/refresh_transcripts.py /tmp/all.log
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
ANSI = re.compile(r"\x1b\[[0-9;]*m")
FENCE = re.compile(r"(下面这份回显来自镜像里的一次实际运行。[^\n]*\n\n```\n)(.*?)(\n```)", re.S)


def transcripts(log):
    found = {}
    pattern = r"======== (\S+) ========\n(.*?)(?=\n======== |\n={10,}|\n TEST RESULTS|\Z)"
    for m in re.finditer(pattern, ANSI.sub("", log), re.S):
        found[m.group(1)] = m.group(2).strip()
    return found


def normalize(text):
    text = re.sub(r"^\[\s*[\d.]+\]\s*", "", text, flags=re.M)
    return re.sub(r"\d+", "#", text)


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    log = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
    found = transcripts(log)
    if not found:
        raise SystemExit("日志里没有 ======== 标签 ======== 块")
    paths = list(ROOT.glob("ch*/README.md")) + list(ROOT.glob("learn/*/README.md"))
    paths.append(ROOT / "hardcode/README.md")
    changed = []
    for path in sorted(paths):
        text = path.read_text(encoding="utf-8")

        def repl(m):
            labels = re.findall(r"^\[PASS\] (\S+)\s*$", m.group(2), re.M)
            if not labels:
                return m.group(0)
            new = found.get(labels[-1])
            if new is None:
                raise SystemExit(f"{path}: 日志里没有 {labels[-1]}")
            if normalize(m.group(2).strip()) == normalize(new):
                return m.group(0)
            changed.append(path.relative_to(ROOT))
            return m.group(1) + new + m.group(3)

        updated = FENCE.sub(repl, text)
        if updated != text:
            path.write_text(updated, encoding="utf-8")
    print(f"{len(changed)} 个 README 的回显有变化")
    for item in changed:
        print(" ", item)


if __name__ == "__main__":
    main()
