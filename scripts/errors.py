#!/usr/bin/env python3
"""
Detect potential error handling issues:
- Unchecked return values from non-void functions
- Missing NULL checks after malloc/calloc/realloc
"""

import sys
from common import get_c_parser, walk_c_files, write_output, PROJECT_ROOT

# Functions known to return void or where ignoring return is OK
VOID_FUNCS = {
    'free', 'memset', 'memcpy', 'memmove', 'bzero', 'qsort', 'va_start', 'va_end',
    'exit', '_exit', 'abort', 'assert', 'perror', 'close', 'fclose', 'shutdown',
    'printf', 'fprintf', 'snprintf', 'sprintf', 'vsnprintf', 'puts', 'fputs', 'putc', 'fputc',
    'strcat', 'strcpy', 'strncpy', 'strncat',
    'SDL_Quit', 'SDL_DestroyWindow', 'SDL_DestroyRenderer', 'SDL_DestroyTexture',
    'SDL_Delay', 'SDL_PumpEvents', 'SDL_SetRenderDrawColor', 'SDL_RenderClear',
    'SDL_RenderPresent', 'SDL_FreeSurface', 'SDL_RenderFillRect', 'SDL_ShowWindow',
    'SDL_SetWindowAlwaysOnTop', 'SDL_RaiseWindow', 'SDL_SetHint', 'SDL_UpdateTexture',
    'SDL_RenderCopy', 'stbi_image_free',
}

internal_void_funcs = set()


def is_void_func(name):
    if name in VOID_FUNCS or name in internal_void_funcs:
        return True
    if name.startswith('_'):
        return True
    return False


def discover_void_funcs(node, source_code):
    """Find all void functions in the codebase."""
    if node.type == 'function_definition':
        type_node = node.child_by_field_name('type')
        if type_node:
            type_text = source_code[type_node.start_byte:type_node.end_byte].decode('utf-8')
            if type_text == 'void':
                from common import get_function_name
                name = get_function_name(node, source_code)
                if name:
                    internal_void_funcs.add(name)
    for child in node.children:
        discover_void_funcs(child, source_code)


def check_errors(node, source_code, stats):
    """Check for error handling issues."""
    problems = []
    
    if node.type == 'expression_statement':
        if node.child_count > 0 and node.children[0].type == 'call_expression':
            call_expr = node.children[0]
            func_node = call_expr.child_by_field_name('function')
            if func_node:
                func_name = source_code[func_node.start_byte:func_node.end_byte].decode('utf-8')
                stats['calls_checked'] += 1
                if not is_void_func(func_name):
                    line = node.start_point.row + 1
                    problems.append(f"L{line}r?({func_name})")
                    
    if node.type == 'compound_statement':
        children = node.children
        pending_allocs = []
        
        for i in range(len(children)):
            curr = children[i]
            alloc_info = None
            
            if curr.type == 'expression_statement' and curr.child_count > 0:
                expr = curr.children[0]
                if expr.type == 'assignment_expression':
                    rhs = expr.child_by_field_name('right')
                    if rhs and rhs.type == 'call_expression':
                        func = rhs.child_by_field_name('function')
                        if func:
                            fn = source_code[func.start_byte:func.end_byte].decode('utf-8')
                            if fn in ('malloc', 'calloc', 'realloc'):
                                lhs = expr.child_by_field_name('left')
                                if lhs:
                                    var = source_code[lhs.start_byte:lhs.end_byte].decode('utf-8')
                                    alloc_info = (curr.start_point.row + 1, var, i)
                                    stats['allocs_checked'] += 1
            
            if curr.type == 'declaration':
                for decl in curr.children:
                    if decl.type == 'init_declarator':
                        init = decl.child_by_field_name('value')
                        if init and init.type == 'call_expression':
                            func = init.child_by_field_name('function')
                            if func:
                                fn = source_code[func.start_byte:func.end_byte].decode('utf-8')
                                if fn in ('malloc', 'calloc', 'realloc'):
                                    declarator = decl.child_by_field_name('declarator')
                                    if declarator:
                                        raw = source_code[declarator.start_byte:declarator.end_byte].decode('utf-8')
                                        var = raw.lstrip('* \t')
                                        alloc_info = (curr.start_point.row + 1, var, i)
                                        stats['allocs_checked'] += 1
            
            if alloc_info:
                pending_allocs.append(alloc_info)
            
            if curr.type == 'if_statement' and pending_allocs:
                cond = curr.child_by_field_name('condition')
                if cond:
                    cond_text = source_code[cond.start_byte:cond.end_byte].decode('utf-8')
                    pending_allocs = [(l, v, idx) for l, v, idx in pending_allocs if v not in cond_text]
            
            if curr.type in ('return_statement', 'goto_statement'):
                pending_allocs = []
        
        for line, var, _ in pending_allocs:
            problems.append(f"L{line}n?({var})")
                    
    for child in node.children:
        problems.extend(check_errors(child, source_code, stats))
        
    return problems


def analyze():
    """Analyze error handling patterns."""
    parser = get_c_parser()
    output = []
    seen_files = {}
    stats = {'calls_checked': 0, 'allocs_checked': 0, 'files': 0}
    
    file_list = []
    for file_path, rel_path, source_code in walk_c_files(PROJECT_ROOT):
        file_list.append((file_path, rel_path, source_code))

    # Pass 1: Discover internal void functions
    for file_path, rel_path, source_code in file_list:
        tree = parser.parse(source_code)
        discover_void_funcs(tree.root_node, source_code)

    # Pass 2: Check errors
    for file_path, rel_path, source_code in file_list:
        tree = parser.parse(source_code)
        results = check_errors(tree.root_node, source_code, stats)
        stats['files'] += 1
        if results:
            seen_files[rel_path] = results

    if seen_files:
        for path, errs in sorted(seen_files.items()):
            output.append(f"{path}: {' '.join(errs)}")
    else:
        output.append(f"# OK: {stats['files']} files, {stats['allocs_checked']} allocs, {stats['calls_checked']} calls checked")

    return output


def main():
    output = analyze()
    if len(sys.argv) > 1 and sys.argv[1] == '-':
        print('\n'.join(output))
    else:
        write_output('errors.txt', output)


if __name__ == "__main__":
    main()
