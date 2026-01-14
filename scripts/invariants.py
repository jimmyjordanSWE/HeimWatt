#!/usr/bin/env python3
"""
Track state variable transitions (state/status/mode assignments).
"""

import sys
from common import get_c_parser, walk_c_files, write_output, PROJECT_ROOT

STATE_KEYWORDS = ('state', 'status', 'mode', 'phase', 'active', 'enabled', 
                  'initialized', 'running', 'paused', 'ready', 'valid', 'dirty')


def find_transitions(node, source_code, transitions):
    """Find state variable assignments."""
    if node.type == 'assignment_expression':
        left = node.child_by_field_name('left')
        right = node.child_by_field_name('right')
        
        if left and right:
            lhs_name = source_code[left.start_byte:left.end_byte].decode('utf-8')
            rhs_name = source_code[right.start_byte:right.end_byte].decode('utf-8')
            
            lhs_lower = lhs_name.lower()
            is_state_var = any(kw in lhs_lower for kw in STATE_KEYWORDS)
            
            if is_state_var:
                is_enum = rhs_name.isupper() and len(rhs_name) > 1 and not rhs_name.isdigit()
                is_bool = rhs_name in ('true', 'false', '0', '1')
                is_null = rhs_name == 'NULL'
                
                if is_enum or is_bool or is_null:
                    line = node.start_point.row + 1
                    transitions.append(f"{lhs_name}->{rhs_name}(L{line})")
    
    if node.type == 'call_expression':
        func = node.child_by_field_name('function')
        if func:
            func_name = source_code[func.start_byte:func.end_byte].decode('utf-8')
            if 'set_state' in func_name.lower() or 'set_status' in func_name.lower():
                args = node.child_by_field_name('arguments')
                if args:
                    arg_text = source_code[args.start_byte:args.end_byte].decode('utf-8')
                    line = node.start_point.row + 1
                    transitions.append(f"{func_name}{arg_text}(L{line})")

    for child in node.children:
        find_transitions(child, source_code, transitions)


def analyze():
    """Analyze state transitions."""
    parser = get_c_parser()
    output = []
    all_transitions = {}
    
    for file_path, rel_path, source_code in walk_c_files(PROJECT_ROOT):
        tree = parser.parse(source_code)
        results = []
        find_transitions(tree.root_node, source_code, results)
        if results:
            all_transitions[rel_path] = results

    output.append("# State Transitions")
    total = 0
    for path, trans in sorted(all_transitions.items()):
        output.append(f"{path}: {' '.join(trans)}")
        total += len(trans)
    output.append(f"# Total: {total} transitions in {len(all_transitions)} files")

    return output


def main():
    output = analyze()
    if len(sys.argv) > 1 and sys.argv[1] == '-':
        print('\n'.join(output))
    else:
        write_output('invariants.txt', output)


if __name__ == "__main__":
    main()
