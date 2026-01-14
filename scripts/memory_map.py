#!/usr/bin/env python3
"""
Map all memory allocation and deallocation calls.
Shows malloc/calloc/realloc/free locations with totals.
"""

import sys
from common import get_c_parser, walk_c_files, write_output, PROJECT_ROOT


def find_allocations(node, source_code, rel_path):
    """Find all allocation/deallocation calls."""
    allocs = []
    
    if node.type == 'call_expression':
        func_node = node.child_by_field_name('function')
        if func_node:
            func_name = source_code[func_node.start_byte:func_node.end_byte].decode('utf-8')
            if func_name in ('malloc', 'calloc', 'realloc', 'free'):
                line = node.start_point.row + 1
                allocs.append((func_name, line))

    for child in node.children:
        allocs.extend(find_allocations(child, source_code, rel_path))
        
    return allocs


def analyze():
    """Analyze memory allocation patterns."""
    parser = get_c_parser()
    output = []
    seen_files = {}
    
    for file_path, rel_path, source_code in walk_c_files(PROJECT_ROOT, extensions=('.c', '.h')):
        tree = parser.parse(source_code)
        allocs = find_allocations(tree.root_node, source_code, rel_path)
        if allocs:
            seen_files[rel_path] = allocs

    output.append("# Legend: [m]alloc [c]alloc [r]ealloc [f]ree  Format: line[op]")
    
    total_allocs = 0
    total_frees = 0
    for path, allocs in sorted(seen_files.items()):
        formatted = []
        for func, line in allocs:
            op = {'malloc':'m','calloc':'c','realloc':'r','free':'f'}.get(func, func)
            formatted.append(f"{line}{op}")
            if func == 'free':
                total_frees += 1
            else:
                total_allocs += 1
        output.append(f"{path}: {' '.join(formatted)}")
    
    output.append(f"# Mem: {total_allocs}a, {total_frees}f ({len(seen_files)} files)")
    
    return output


def main():
    output = analyze()
    if len(sys.argv) > 1 and sys.argv[1] == '-':
        print('\n'.join(output))
    else:
        write_output('memory_map.txt', output)


if __name__ == "__main__":
    main()
