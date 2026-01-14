#!/usr/bin/env python3
"""
Extract hierarchical structure of C codebase: functions, structs, enums, typedefs.
Output is optimized for LLM consumption with smart prefix-clustering.
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


def extract_symbols(node, source_code):
    """Extract all symbols (functions, structs, enums, typedefs) from AST."""
    symbols = []
    loc = node.end_point[0] - node.start_point[0] + 1
    
    if node.type == 'function_definition':
        for child in node.children:
            if child.type == 'function_declarator':
                name, params = extract_fn_info(child, source_code)
                if name:
                    symbols.append({'type': 'fn', 'repr': f"{name}({','.join(params)})", 
                                   'loc': loc, 'is_def': True})

    elif node.type == 'declaration':
        for child in node.children:
            if child.type == 'function_declarator':
                name, params = extract_fn_info(child, source_code)
                if name:
                    symbols.append({'type': 'fn', 'repr': f"{name}({','.join(params)})", 
                                   'loc': loc, 'is_def': False})

    elif node.type == 'struct_specifier':
        name = ""
        fields = []
        for child in node.children:
            if child.type in ('type_identifier', 'identifier'):
                name = source_code[child.start_byte:child.end_byte].decode('utf-8')
            if child.type == 'field_declaration_list':
                for field in child.children:
                    if field.type == 'field_declaration':
                        f_name = ""
                        f_ptr = False
                        for f_child in field.children:
                            if f_child.type in ('field_identifier', 'identifier', 'pointer_declarator',
                                               'array_declarator', 'function_declarator', 
                                               'parenthesized_declarator'):
                                f_name, f_ptr = get_name_and_pointer_status(f_child, source_code)
                                break
                        if f_name:
                            prefix = "*" if f_ptr else ""
                            fields.append(f"{prefix}{f_name}")

        if name or fields:
            display_name = name if name else "<anon>"
            field_str = "{" + ",".join(fields) + "}" if fields else ""
            symbols.append({'type': 'st', 'repr': f"{display_name}{field_str}", 'loc': loc})

    elif node.type == 'enum_specifier':
        for child in node.children:
            if child.type == 'type_identifier':
                name = source_code[child.start_byte:child.end_byte].decode('utf-8')
                symbols.append({'type': 'en', 'repr': name, 'loc': loc})
                break

    elif node.type == 'type_definition':
        name = ""
        for child in node.children:
            if child.type == 'type_identifier':
                name = source_code[child.start_byte:child.end_byte].decode('utf-8')
        if name:
            symbols.append({'type': 'ty', 'repr': name, 'loc': loc})

    for child in node.children:
        if node.type not in ('compound_statement', 'field_declaration_list', 'parameter_list'):
            symbols.extend(extract_symbols(child, source_code))
    
    return symbols


def analyze():
    """Analyze codebase and return structured output lines."""
    parser = get_c_parser()
    output = ["STRUCTURE"]
    tree = {}
    
    for file_path, rel_path, source_code in walk_c_files(PROJECT_ROOT, extensions=('.c', '.h')):
        parts = rel_path.split('/')
        curr = tree
        for part in parts:
            if part not in curr: curr[part] = {}
            curr = curr[part]
        ast_tree = parser.parse(source_code)
        curr['__symbols__'] = extract_symbols(ast_tree.root_node, source_code)
        curr['__loc__'] = len(source_code.splitlines())

    sym_type = ""
    
    def print_hierarchical(syms, indent):
        if not syms: return
        
        trie = {}
        for s in syms:
            if '(' in s['repr']:
                name, suffix = s['repr'].split('(', 1)
                suffix = '(' + suffix
            elif '{' in s['repr']:
                name, suffix = s['repr'].split('{', 1)
                suffix = '{' + suffix
            else:
                name = s['repr']
                suffix = ""
            
            parts = []
            current = ""
            for char in name:
                current += char
                if char == '_':
                    parts.append(current)
                    current = ""
            if current:
                parts.append(current)
            
            curr = trie
            for i, part in enumerate(parts):
                if part not in curr: curr[part] = {}
                curr = curr[part]
                if i == len(parts) - 1:
                    if '__leaf__' not in curr: curr['__leaf__'] = []
                    curr['__leaf__'].append((suffix, s['loc']))

        def _print_trie(node, prefix_str, current_indent, needs_type=True):
            children = [k for k in node if k != '__leaf__']
            
            if len(children) == 1 and '__leaf__' not in node:
                child_key = children[0]
                _print_trie(node[child_key], prefix_str + child_key, current_indent, needs_type)
                return

            type_tag = f"{sym_type} " if needs_type else ""

            if prefix_str:
                if '__leaf__' in node and not children and len(node['__leaf__']) == 1:
                    sf, _ = node['__leaf__'][0]
                    output.append(" " * current_indent + f"{type_tag}{prefix_str}{sf}")
                else:
                    output.append(" " * current_indent + f"{type_tag}{prefix_str}")
                    for leaf in node.get('__leaf__', []):
                        sf, _ = leaf
                        output.append(" " * (current_indent + 1) + f"{sf}")
                    for child_key in sorted(children):
                        _print_trie(node[child_key], child_key, current_indent + 1, needs_type=False)
            else:
                for leaf in node.get('__leaf__', []):
                    sf, _ = leaf
                    output.append(" " * current_indent + f"{sym_type} {sf}")
                for child_key in sorted(children):
                    _print_trie(node[child_key], child_key, current_indent, needs_type=True)

        _print_trie(trie, "", indent, needs_type=True)

    def print_tree(node, name, indent=0):
        nonlocal sym_type
        if name == '__symbols__': return
        output.append(" " * indent + name)
        if '__symbols__' in node:
            syms = node['__symbols__']
            for group_type in ['fn', 'st', 'ty', 'en']:
                type_map = {'fn':'f', 'st':'s', 'ty':'t', 'en':'e'}
                
                unique_syms = {}
                for s in syms:
                    if s['type'] != group_type: continue
                    r = s['repr']
                    if r not in unique_syms:
                        unique_syms[r] = s
                    else:
                        cur = unique_syms[r]
                        s_def = s.get('is_def', False)
                        cur_def = cur.get('is_def', False)
                        if s_def and not cur_def:
                            unique_syms[r] = s
                        elif s_def == cur_def and s['loc'] > cur['loc']:
                            unique_syms[r] = s
                
                group_syms = list(unique_syms.values())
                if group_syms:
                    sym_type = type_map[group_type]
                    print_hierarchical(group_syms, indent + 1)
        
        for child_name in sorted(node.keys()):
            if child_name not in ('__symbols__', '__loc__'):
                print_tree(node[child_name], child_name, indent + 1)

    for top_level in sorted(tree.keys()):
        print_tree(tree[top_level], top_level, 0)

    return output


def main():
    output = analyze()
    
    if len(sys.argv) > 1 and sys.argv[1] == '-':
        print('\n'.join(output))
    else:
        write_output('structure.txt', output)


if __name__ == "__main__":
    main()
