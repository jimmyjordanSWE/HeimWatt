#!/usr/bin/env python3
"""
Find performance hotspots: nested loops and allocations inside loops.
"""

import sys
from common import get_c_parser, walk_c_files, write_output, get_function_name, PROJECT_ROOT


def find_hotspots(node, source_code, curr_func):
    """Find nested loops and loop allocations."""
    hotspots = []
    
    if node.type == 'function_definition':
        name = get_function_name(node, source_code)
        if name:
            curr_func = name
    
    # Check for nested loops (O(n^2))
    if node.type in ('for_statement', 'while_statement', 'do_statement'):
        p = node.parent
        depth = 0
        while p:
            if p.type in ('for_statement', 'while_statement', 'do_statement'):
                depth += 1
            p = p.parent
            
        if depth >= 1:
            line = node.start_point.row + 1
            hotspots.append(f"{curr_func}:LOOP({depth+1},L{line})")

    # Check for frequent allocations (alloc inside loop)
    if node.type == 'call_expression':
        func_node = node.child_by_field_name('function')
        if func_node:
            func_name = source_code[func_node.start_byte:func_node.end_byte].decode('utf-8')
            if func_name in ('malloc', 'calloc', 'realloc'):
                p = node.parent
                in_loop = False
                while p:
                    if p.type in ('for_statement', 'while_statement', 'do_statement'):
                        in_loop = True
                        break
                    p = p.parent
                 
                if in_loop:
                    line = node.start_point.row + 1
                    hotspots.append(f"{curr_func}:ALLOC_LOOP(L{line})")

    for child in node.children:
        hotspots.extend(find_hotspots(child, source_code, curr_func))
        
    return hotspots


def analyze():
    """Analyze for performance hotspots."""
    parser = get_c_parser()
    output = ["Hotspots:"]
    
    for file_path, rel_path, source_code in walk_c_files(PROJECT_ROOT):
        tree = parser.parse(source_code)
        results = find_hotspots(tree.root_node, source_code, None)
        if results:
            output.append(f"{rel_path}: {' '.join(results)}")

    if len(output) == 1:
        output.append("# No hotspots found")
    
    return output


def main():
    output = analyze()
    if len(sys.argv) > 1 and sys.argv[1] == '-':
        print('\n'.join(output))
    else:
        write_output('hotspots.txt', output)


if __name__ == "__main__":
    main()
