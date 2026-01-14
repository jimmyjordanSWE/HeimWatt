#!/usr/bin/env python3
"""
List functions sorted by lines of code (LOC).
Helps identify complexity hotspots for refactoring.
"""

import sys
from common import get_c_parser, walk_c_files, write_output, PROJECT_ROOT


def get_name_and_pointer_status(node, source_code):
    """Recursively finds the identifier and whether it is a pointer."""
    name = ""
    is_ptr = False
    
    if node.type in ('field_identifier', 'identifier'):
        name = source_code[node.start_byte:node.end_byte].decode('utf-8')
    elif node.type == 'pointer_declarator':
        is_ptr = True
        for child in node.children:
            sub_name, sub_ptr = get_name_and_pointer_status(child, source_code)
            if sub_name: name = sub_name
            if sub_ptr: is_ptr = True
    elif node.type in ('parenthesized_declarator', 'array_declarator', 'function_declarator'):
        for child in node.children:
            sub_name, sub_ptr = get_name_and_pointer_status(child, source_code)
            if sub_name: name = sub_name
            if sub_ptr: is_ptr = True
            
    return name, is_ptr


def extract_fn_info(node, source_code):
    """Extracts name and params from a function_declarator node."""
    name = ""
    params = []
    
    for child in node.children:
        if child.type in ('identifier', 'pointer_declarator', 'parenthesized_declarator'):
            sub_name, _ = get_name_and_pointer_status(child, source_code)
            if sub_name: name = sub_name
        
        if child.type == 'parameter_list':
            for p in child.children:
                if p.type == 'parameter_declaration':
                    p_name = ""
                    p_ptr = False
                    for p_child in p.children:
                        if p_child.type in ('identifier', 'pointer_declarator', 'array_declarator',
                                           'function_declarator', 'parenthesized_declarator'):
                            p_name, p_ptr = get_name_and_pointer_status(p_child, source_code)
                            break
                    if p_name:
                        prefix = "*" if p_ptr else ""
                        params.append(f"{prefix}{p_name}")
                    else:
                        content = source_code[p.start_byte:p.end_byte].decode('utf-8').strip()
                        if '*' in content: params.append("*_")
                        else: params.append("_")
    return name, params


def extract_functions(node, source_code):
    """Extract function definitions with LOC."""
    functions = []
    
    if node.type == 'function_definition':
        loc = node.end_point[0] - node.start_point[0] + 1
        for child in node.children:
            if child.type == 'function_declarator':
                name, params = extract_fn_info(child, source_code)
                if name:
                    functions.append({
                        'repr': f"{name}({','.join(params)})",
                        'loc': loc
                    })

    for child in node.children:
        if node.type != 'compound_statement':
            functions.extend(extract_functions(child, source_code))
    
    return functions


def analyze():
    """Analyze and list long functions."""
    parser = get_c_parser()
    output = ["LONG FUNCTIONS"]
    all_fns = []
    
    for file_path, rel_path, source_code in walk_c_files(PROJECT_ROOT, extensions=('.c', '.h')):
        tree = parser.parse(source_code)
        functions = extract_functions(tree.root_node, source_code)
        for fn in functions:
            if fn['loc'] > 10:
                all_fns.append({
                    'file': rel_path,
                    'repr': fn['repr'],
                    'loc': fn['loc']
                })
    
    # Sort by LOC descending
    all_fns.sort(key=lambda x: x['loc'], reverse=True)
    
    for fn in all_fns:
        output.append(f"{fn['file']}: {fn['repr']} LOC: {fn['loc']}")
    
    return output


def main():
    output = analyze()
    if len(sys.argv) > 1 and sys.argv[1] == '-':
        print('\n'.join(output))
    else:
        write_output('long_functions.txt', output)


if __name__ == "__main__":
    main()
