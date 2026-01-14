"""
Common utilities for C codebase analysis scripts.
Provides tree-sitter parser factory and file walking utilities.
"""

import os
from pathlib import Path
from tree_sitter import Language, Parser
import tree_sitter_c as tsc

# Output directory for analysis results
SCRIPT_DIR = Path(__file__).parent
OUTPUT_DIR = SCRIPT_DIR / "out"
PROJECT_ROOT = SCRIPT_DIR.parent

# Default directories to analyze
INCLUDED_DIRS = {'src', 'include'}
C_EXTENSIONS = ('.c', '.h')


def get_c_parser() -> Parser:
    """Create and return a tree-sitter C parser."""
    return Parser(Language(tsc.language()))


def walk_c_files(root: Path = None, extensions: tuple = ('.c',), 
                  included_dirs: set = None) -> tuple:
    """
    Walk C source files in the project.
    
    Yields:
        (file_path, rel_path, source_bytes) for each matching file
    """
    if root is None:
        root = PROJECT_ROOT
    if included_dirs is None:
        included_dirs = INCLUDED_DIRS
    
    for dir_name in sorted(included_dirs):
        dir_path = root / dir_name
        if not dir_path.exists():
            continue
        
        for file_path in sorted(dir_path.rglob('*')):
            if file_path.is_file() and file_path.suffix in extensions:
                rel_path = file_path.relative_to(root)
                with open(file_path, 'rb') as f:
                    yield file_path, str(rel_path), f.read()


def ensure_output_dir():
    """Create output directory if it doesn't exist."""
    OUTPUT_DIR.mkdir(exist_ok=True)


def write_output(filename: str, lines: list):
    """Write output to file in the output directory."""
    ensure_output_dir()
    output_path = OUTPUT_DIR / filename
    with open(output_path, 'w') as f:
        f.write('\n'.join(lines))
        if lines:
            f.write('\n')


def get_function_name(node, source_code: bytes) -> str:
    """Extract function name from a function_definition node."""
    decl = node.child_by_field_name('declarator')
    while decl and decl.type in ('pointer_declarator', 'parenthesized_declarator'):
        if decl.child_count > 0:
            decl = decl.children[0]
    
    if decl and decl.type == 'function_declarator':
        fname_node = decl.child_by_field_name('declarator')
        if fname_node:
            return source_code[fname_node.start_byte:fname_node.end_byte].decode('utf-8')
    return None
