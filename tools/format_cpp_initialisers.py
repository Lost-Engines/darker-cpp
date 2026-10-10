"""Put designated C++ initialisers on separate lines without changing tokens."""
from pathlib import Path
import re

# Preserve C++ tokens, including comments and literals; change whitespace only.
TOKEN = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|(?:u8|u|U|L)?R"([^ ()\\\t\r\n]*)\([\s\S]*?\)\1"|(?:u8|u|U|L)?"(?:\\.|[^"\\])*"|[0-9][\w.\x27]*|(?:u8|u|U|L)?\x27(?:\\.|[^\x27\\\n])*\x27|\w+|[^\s]', re.MULTILINE)

def format_designated_initialisers(source):
    tokens = list(TOKEN.finditer(source))
    stack = []
    parents = {}
    fields = {}
    closes = {}
    previous = None
    for i, token in enumerate(tokens):
        value = token.group()
        if value.startswith(('//', '/*')):
            continue
        if value == '.' and stack and tokens[stack[-1]].group() == '{' and previous is not None and tokens[previous].group() in ('{', ','):
            if i + 2 < len(tokens) and re.fullmatch(r'[A-Za-z_]\w*', tokens[i+1].group()) and tokens[i+2].group() in ('{', '='):
                fields.setdefault(stack[-1], []).append(i)
        if value in ('{', '(', '['):
            parents[i] = stack[-1] if stack else None
            stack.append(i)
        elif value in ('}', ')', ']'):
            if not stack:
                continue
            opening = stack.pop()
            closes[opening] = i
        previous = i
    edits = []
    indents = {}
    for opening, members in fields.items():
        ancestor = parents[opening]
        while ancestor is not None and ancestor not in fields:
            ancestor = parents[ancestor]
        line_start = source.rfind('\n', 0, tokens[opening].start()) + 1
        original_indent = len(source[line_start:]) - len(source[line_start:].lstrip(' \t'))
        indent = indents[ancestor] + 2 if ancestor is not None else original_indent
        indents[opening] = indent
        for member in members:
            edits.append((tokens[member-1].end(), tokens[member].start(), '\n' + ' '*(indent+2)))
        closing = closes[opening]
        edits.append((tokens[closing-1].end(), tokens[closing].start(), '\n' + ' '*indent))
    for start, end, replacement in sorted(edits, reverse=True):
        if source[start:end].strip():
            raise ValueError('Refusing to change non-whitespace')
        source = source[:start] + replacement + source[end:]
    return source

if __name__ == '__main__':
    import sys
    for name in sys.argv[1:]:
        path = Path(name)
        original = path.read_text()
        formatted = format_designated_initialisers(original)
        assert [m.group() for m in TOKEN.finditer(original)] == [m.group() for m in TOKEN.finditer(formatted)], path
        assert format_designated_initialisers(formatted) == formatted, path
        if original != formatted:
            path.write_text(formatted)
