#!/usr/bin/env python3
import json
import os
import glob

def generate():
    project_root = os.getcwd()
    # Common flags from Makefile
    common_flags = [
        "-std=c99",
        "-D_GNU_SOURCE",
        "-D_POSIX_C_SOURCE=200809L",
        "-Wall",
        "-Wextra",
        "-pthread",
        "-I", os.path.join(project_root, "libs"),
        "-I", os.path.join(project_root, "include"),
        "-I", os.path.join(project_root, "src"),
        "-I", project_root,
        "-I", os.path.join(project_root, "libs", "duckdb")
    ]
    
    commands = []
    
    # Find all .c files in src and its subdirectories
    sources = glob.glob("src/**/*.c", recursive=True)
    sources.extend(glob.glob("src/*.c"))
    # Also include plugins
    sources.extend(glob.glob("plugins/**/*.c", recursive=True))
    # Dedup
    sources = list(set(sources))
    
    for src in sources:
        # Construct the command
        cmd_list = ["gcc"] + common_flags + ["-I", os.path.dirname(src), "-c", src]
        
        commands.append({
            "directory": project_root,
            "command": " ".join(cmd_list),
            "file": src
        })
    
    with open("compile_commands.json", "w") as f:
        json.dump(commands, f, indent=4)
    
    print(f"Generated compile_commands.json with {len(commands)} entries.")

if __name__ == "__main__":
    generate()
