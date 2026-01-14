#!/usr/bin/env python3
"""
Build call graph and trace execution paths from main().
Output shows module dependencies and call tree.
"""

import sys
from collections import defaultdict
from common import get_c_parser, walk_c_files, write_output, get_function_name, PROJECT_ROOT


def find_calls(node, source_code, calls_from_function):
    """Find all function calls within a node."""
    if node.type == 'call_expression':
        func_node = node.child_by_field_name('function')
        if func_node:
            func_name = source_code[func_node.start_byte:func_node.end_byte].decode('utf-8')
            calls_from_function.add(func_name)
           
    for child in node.children:
        find_calls(child, source_code, calls_from_function)


def analyze_function(node, source_code, current_function_name, call_graph):
    """Analyze a function definition and build call graph."""
    if node.type == 'function_definition':
        name = get_function_name(node, source_code)
        if name:
            current_function_name = name
    
    if current_function_name:
        calls = set()
        find_calls(node, source_code, calls)
        if calls:
            if current_function_name not in call_graph:
                call_graph[current_function_name] = set()
            call_graph[current_function_name].update(calls)

    if node.type == 'translation_unit':
        for child in node.children:
            analyze_function(child, source_code, None, call_graph)


def analyze():
    """Analyze codebase and return call chain output."""
    parser = get_c_parser()
    call_graph = defaultdict(set)
    func_to_module = {}
    output = []
    
    for file_path, rel_path, source_code in walk_c_files(PROJECT_ROOT):
        module = file_path.stem
        tree = parser.parse(source_code)
        
        # First pass: find all function definitions
        def find_defs(node):
            if node.type == 'function_definition':
                name = get_function_name(node, source_code)
                if name:
                    func_to_module[name] = module
            for child in node.children: 
                find_defs(child)
        find_defs(tree.root_node)
        
        # Second pass: analyze calls
        analyze_function(tree.root_node, source_code, None, call_graph)

    def get_module(fn):
        return func_to_module.get(fn, 'stdlib')
    
    module_graph = defaultdict(set)
    for caller, callees in call_graph.items():
        caller_mod = get_module(caller)
        for callee in callees:
            callee_mod = get_module(callee)
            if callee_mod != caller_mod and callee_mod != 'stdlib':
                module_graph[caller_mod].add(callee_mod)
    
    output.append("Flow:")
    for mod in sorted(module_graph.keys()):
        targets = sorted(module_graph[mod])
        output.append(f" {mod}->{','.join(targets)}")
    
    output.append("")
    output.append("Tree(main):")
    
    global_expanded = set()
    
    def print_tree(fn, indent, path):
        if fn in path:
            output.append(f"{'  '*indent}{fn} (recursive)")
            return
        if fn in global_expanded:
            output.append(f"{'  '*indent}{fn} (see above)")
            return

        callees = []
        if fn in call_graph:
            callees = sorted(call_graph[fn])
            callees = [c for c in callees if c in call_graph or get_module(c) != 'stdlib']
        
        if not callees:
            output.append(f"{'  '*indent}{fn}")
            return

        output.append(f"{'  '*indent}{fn}")
        global_expanded.add(fn)
        new_path = path | {fn}
        for c in callees:
            print_tree(c, indent + 1, new_path)
    
    if 'main' in call_graph:
        print_tree('main', 1, set())
    else:
        output.append("  !main")
    
    return output


def main():
    output = analyze()
    
    if len(sys.argv) > 1 and sys.argv[1] == '-':
        print('\n'.join(output))
    else:
        write_output('call_chains.txt', output)


if __name__ == "__main__":
    main()
