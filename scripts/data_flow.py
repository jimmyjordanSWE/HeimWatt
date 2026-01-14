#!/usr/bin/env python3
"""
Track struct field read/write patterns per module.
Shows data ownership and flow between modules.
"""

import sys
from collections import defaultdict
from common import get_c_parser, walk_c_files, write_output, PROJECT_ROOT


def infer_type(var_name):
    """Extract base variable name from complex expression."""
    return var_name.split('[')[0].split('-')[0].strip('()*& ')


def find_struct_access(node, source_code, type_accesses):
    """Find struct field accesses and track r/w patterns."""
    if node.type == 'field_expression':
        parent = node.parent
        mode = "r"
        if parent and parent.type == 'assignment_expression':
            lhs = parent.child_by_field_name('left')
            if lhs == node:
                mode = "w"
        
        arg = node.child_by_field_name('argument')
        if arg:
            struct_var = source_code[arg.start_byte:arg.end_byte].decode('utf-8')
            struct_type = infer_type(struct_var)
            type_accesses[struct_type][mode] += 1

    for child in node.children:
        find_struct_access(child, source_code, type_accesses)


def analyze():
    """Analyze data flow patterns."""
    parser = get_c_parser()
    output = []
    
    global_types = defaultdict(lambda: {'r': 0, 'w': 0})
    module_types = defaultdict(lambda: defaultdict(lambda: {'r': 0, 'w': 0}))
    
    for file_path, rel_path, source_code in walk_c_files(PROJECT_ROOT):
        module = rel_path.split('/')[0] if '/' in rel_path else 'root'
        tree = parser.parse(source_code)
        
        file_types = defaultdict(lambda: {'r': 0, 'w': 0})
        find_struct_access(tree.root_node, source_code, file_types)
        
        for t, counts in file_types.items():
            global_types[t]['r'] += counts['r']
            global_types[t]['w'] += counts['w']
            module_types[module][t]['r'] += counts['r']
            module_types[module][t]['w'] += counts['w']
    
    output.append("Data Ownership (struct r/w totals):")
    sorted_types = sorted(global_types.items(), key=lambda x: x[1]['r']+x[1]['w'], reverse=True)
    for t, counts in sorted_types[:15]:
        if counts['r'] + counts['w'] >= 5:
            output.append(f"  {t}: r({counts['r']}) w({counts['w']})")
    
    output.append("")
    output.append("Module Data Usage:")
    for mod in sorted(module_types.keys()):
        types = module_types[mod]
        top = sorted(types.items(), key=lambda x: x[1]['r']+x[1]['w'], reverse=True)[:5]
        type_strs = [f"{t}:r{c['r']}w{c['w']}" for t,c in top if c['r']+c['w'] >= 3]
        if type_strs:
            output.append(f"  {mod}: {', '.join(type_strs)}")

    return output


def main():
    output = analyze()
    if len(sys.argv) > 1 and sys.argv[1] == '-':
        print('\n'.join(output))
    else:
        write_output('data_flow.txt', output)


if __name__ == "__main__":
    main()
