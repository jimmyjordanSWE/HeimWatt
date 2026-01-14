#!/usr/bin/env python3
"""
Count tokens using tiktoken encoders for various LLM models.
Provides breakdown by directory and SLOC count.
"""

import sys
import re
from collections import defaultdict

try:
    import tiktoken
    HAS_TIKTOKEN = True
except ImportError:
    HAS_TIKTOKEN = False

from common import walk_c_files, write_output, PROJECT_ROOT, INCLUDED_DIRS


def count_tokens(text, encoding_name):
    """Count tokens using tiktoken."""
    if not HAS_TIKTOKEN:
        return len(text) // 4  # Rough estimate
    try:
        enc = tiktoken.get_encoding(encoding_name)
    except Exception:
        enc = tiktoken.get_encoding("cl100k_base")
    return len(enc.encode(text))


def get_sloc(content):
    """Count source lines of code (excluding comments and blanks)."""
    content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
    content = re.sub(r'//.*', '', content)
    return sum(1 for line in content.splitlines() if line.strip())


def analyze():
    """Analyze token counts and return output lines."""
    output = []
    total_chars = 0
    total_lines = 0
    file_contents = []
    dir_stats = defaultdict(lambda: {"files": 0, "lines": 0})
    
    for file_path, rel_path, source_bytes in walk_c_files(PROJECT_ROOT, extensions=('.c', '.h')):
        try:
            content = source_bytes.decode('utf-8', errors='ignore')
            lines = get_sloc(content)
            
            total_chars += len(content)
            total_lines += lines
            
            # Get top-level directory
            dir_name = rel_path.split('/')[0] if '/' in rel_path else 'root'
            dir_stats[dir_name]["files"] += 1
            dir_stats[dir_name]["lines"] += lines
            file_contents.append(content)
        except Exception:
            pass

    if not file_contents:
        output.append("TOKEN ANALYSIS")
        output.append("No files found in included directories.")
        return output

    full_text = "\n".join(file_contents)
    
    gpt_tokens = count_tokens(full_text, "o200k_base")
    claude_tokens = count_tokens(full_text, "cl100k_base")
    
    output.append("TOKEN ANALYSIS")
    output.append(f"Files: {len(file_contents)}")
    output.append(f"SLOC: {total_lines}")
    output.append(f"Chars: {total_chars}")
    output.append("-" * 30)
    output.append(f"o200k (GPT-4o): {gpt_tokens} tokens")
    output.append(f"cl100k (Claude): {claude_tokens} tokens")
    output.append("-" * 30)
    output.append("BY DIRECTORY:")
    
    for d in sorted(INCLUDED_DIRS):
        stats = dir_stats.get(d, {"files": 0, "lines": 0})
        if stats["files"] > 0:
            output.append(f"  {d:15}: {stats['lines']:5} lines ({stats['files']:3} files)")

    return output


def main():
    output = analyze()
    
    if len(sys.argv) > 1 and sys.argv[1] == '-':
        print('\n'.join(output))
    else:
        write_output('token_count.txt', output)


if __name__ == "__main__":
    main()
