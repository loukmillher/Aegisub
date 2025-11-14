#!/usr/bin/env python3
"""
Complete library dependency fixer for macOS app bundles.
This script makes multiple passes to fix ALL dependencies, including those in copied libraries.
"""

import re
import sys
import os
import shutil
import stat
import subprocess
from pathlib import Path

is_bad_lib = re.compile(r'(/usr/local|/opt)').match
is_sys_lib = re.compile(r'(/usr|/System)').match
otool_libname_extract = re.compile(r'\s+([/@].*?)[(\s:]').search

def otool(cmdline):
    """Run otool command and return output lines."""
    with subprocess.Popen(['otool'] + cmdline, stdout=subprocess.PIPE,
                          encoding='utf-8', stderr=subprocess.PIPE) as p:
        stdout, stderr = p.communicate()
        if p.returncode != 0:
            print(f"Warning: otool failed: {stderr}")
            return []
        return stdout.splitlines()


def get_rpath(lib):
    """Extract rpath entries from a library."""
    info = otool(['-l', lib])
    commands = []
    command = []
    for line in info:
        line = line.strip()
        if line.startswith("Load command "):
            commands.append(command)
            command = []
        else:
            command.append(line)
    commands.append(command)

    return [line.split()[1] for command in commands 
            if "cmd LC_RPATH" in command 
            for line in command if line.startswith("path")]


def get_dependencies(lib):
    """Get all library dependencies of a binary/library."""
    deps = []
    liblist = otool(['-L', lib])
    
    for line in liblist:
        match = otool_libname_extract(line)
        if not match:
            continue
        
        dep = match.group(1)
        if dep == lib or os.path.basename(dep) == os.path.basename(lib):
            # Skip self-reference
            continue
            
        deps.append(dep)
    
    return deps


def resolve_path(dep, binary_path):
    """Resolve @rpath and @loader_path references."""
    if dep.startswith("@rpath/"):
        rpath = get_rpath(binary_path)
        if not rpath:
            print(f"WARNING: {binary_path} uses @rpath but has no rpath set for {dep}")
            return dep
        
        # Try each rpath entry
        for rp in rpath:
            resolved = os.path.join(rp, dep[len("@rpath/"):])
            if os.path.exists(resolved):
                return resolved
        
        # If nothing found, use first rpath entry
        return os.path.join(rpath[0], dep[len("@rpath/"):])
    
    if dep.startswith("@loader_path/"):
        return os.path.join(os.path.dirname(binary_path), dep[len("@loader_path/"):])
    
    if dep.startswith("@executable_path/"):
        # Already fixed, return as-is
        return dep
    
    return dep


def copy_library_with_symlinks(source_path, target_dir):
    """Copy a library and its symlinks to target directory."""
    copied_files = []
    
    # Follow symlink chain
    current = source_path
    symlink_chain = []
    visited = set()
    
    while os.path.islink(current):
        if current in visited:
            print(f"    WARNING: Circular symlink detected: {current}")
            break
        visited.add(current)
        
        link_target = os.readlink(current)
        symlink_chain.append((os.path.basename(current), link_target))
        
        # Resolve relative symlinks
        if not os.path.isabs(link_target):
            current = os.path.join(os.path.dirname(current), link_target)
        else:
            current = link_target
    
    # Copy the actual file
    if os.path.isfile(current):
        basename = os.path.basename(current)
        target_file = os.path.join(target_dir, basename)
        
        if not os.path.exists(target_file):
            shutil.copy2(current, target_file)
            print(f"    Copied: {basename}")
            copied_files.append(basename)
        else:
            print(f"    Exists: {basename}")
    
    # Create symlinks
    for link_name, link_target in reversed(symlink_chain):
        target_link = os.path.join(target_dir, link_name)
        
        # Use basename for relative symlinks
        if not os.path.isabs(link_target):
            link_target = os.path.basename(link_target)
        
        if not os.path.exists(target_link):
            os.symlink(link_target, target_link)
            print(f"    Linked: {link_name} -> {link_target}")
            copied_files.append(link_name)
    
    return copied_files


def find_all_bad_libs(target_dir):
    """Find all Homebrew/local library references in the bundle."""
    bad_libs = {}  # {original_path: [binaries that reference it]}
    
    # Find all binaries and libraries
    binaries = []
    for item in os.listdir(target_dir):
        full_path = os.path.join(target_dir, item)
        if os.path.isfile(full_path) and not os.path.islink(full_path):
            if item.endswith('.dylib') or os.access(full_path, os.X_OK):
                binaries.append(full_path)
    
    # Check each binary
    for binary in binaries:
        deps = get_dependencies(binary)
        for dep in deps:
            resolved = resolve_path(dep, binary)
            
            if is_bad_lib(resolved):
                if resolved not in bad_libs:
                    bad_libs[resolved] = []
                bad_libs[resolved].append(binary)
    
    return bad_libs


def fix_install_names(target_dir):
    """Fix install names for all binaries in target directory."""
    binaries = []
    for item in os.listdir(target_dir):
        full_path = os.path.join(target_dir, item)
        if os.path.isfile(full_path) and not os.path.islink(full_path):
            if item.endswith('.dylib') or os.access(full_path, os.X_OK):
                binaries.append(full_path)
    
    for binary in binaries:
        basename = os.path.basename(binary)
        deps = get_dependencies(binary)
        
        changes = []
        needs_id_fix = False
        
        # Check if the library's own ID needs fixing
        if basename.endswith('.dylib'):
            id_lines = otool(['-D', binary])
            if len(id_lines) > 1:
                current_id = id_lines[1].strip()
                if is_bad_lib(current_id) or '@rpath' in current_id or '@loader_path' in current_id:
                    needs_id_fix = True
        
        # Collect all changes needed
        for dep in deps:
            resolved = resolve_path(dep, binary)
            dep_basename = os.path.basename(resolved)
            
            # Check if we should change this dependency
            should_change = False
            
            if is_bad_lib(resolved):
                should_change = True
            elif dep.startswith("@rpath/"):
                should_change = True
            elif dep.startswith("@loader_path/"):
                should_change = True
            
            if should_change and not dep.startswith("@executable_path/"):
                target_path = f"@executable_path/{dep_basename}"
                changes.append((dep, target_path))
        
        # Apply changes
        if changes or needs_id_fix:
            # Make writable
            orig_permission = os.stat(binary).st_mode
            if not (orig_permission & stat.S_IWUSR):
                os.chmod(binary, orig_permission | stat.S_IWUSR)
            
            # Fix install name (for libraries)
            if needs_id_fix:
                subprocess.run(['install_name_tool', '-id', 
                              f'@executable_path/{basename}', binary],
                             stderr=subprocess.DEVNULL)
                print(f"  Fixed ID in {basename}")
            
            # Fix dependencies
            for old_path, new_path in changes:
                result = subprocess.run(['install_name_tool', '-change', 
                                       old_path, new_path, binary],
                                      capture_output=True)
                if result.returncode == 0:
                    print(f"  Fixed in {basename}: {os.path.basename(old_path)}")
            
            # Restore permissions
            if not (orig_permission & stat.S_IWUSR):
                os.chmod(binary, orig_permission)


def main():
    if len(sys.argv) < 2:
        print("Usage: osx-fix-libs-complete.py <path-to-binary>")
        sys.exit(1)
    
    binname = sys.argv[1]
    target_dir = os.path.dirname(binname)
    
    if not os.path.exists(binname):
        print(f"ERROR: Binary not found: {binname}")
        sys.exit(1)
    
    print(f"Fixing library dependencies for: {binname}")
    print(f"Target directory: {target_dir}")
    print()
    
    max_passes = 10
    for pass_num in range(1, max_passes + 1):
        print(f"=== PASS {pass_num} ===")
        
        # Find all bad library references
        bad_libs = find_all_bad_libs(target_dir)
        
        if not bad_libs:
            print("✓ No Homebrew/local dependencies found!")
            break
        
        print(f"Found {len(bad_libs)} external libraries to copy:")
        
        # Copy missing libraries
        for lib_path in sorted(bad_libs.keys()):
            if os.path.exists(lib_path):
                print(f"\n  {lib_path}")
                try:
                    copy_library_with_symlinks(lib_path, target_dir)
                except Exception as e:
                    print(f"    ERROR: Failed to copy: {e}")
            else:
                print(f"\n  WARNING: Library not found: {lib_path}")
        
        print("\nFixing install names...")
        fix_install_names(target_dir)
        print()
        
        if pass_num == max_passes:
            print(f"WARNING: Reached maximum passes ({max_passes})")
            print("Some dependencies may still remain.")
    
    # Final verification
    print("\n=== FINAL VERIFICATION ===")
    remaining = find_all_bad_libs(target_dir)
    
    if remaining:
        print(f"⚠ WARNING: {len(remaining)} external dependencies still remain:")
        for lib in sorted(remaining.keys()):
            print(f"  {lib}")
            for binary in remaining[lib]:
                print(f"    referenced by: {os.path.basename(binary)}")
    else:
        print("✓ All dependencies resolved!")
        print("✓ App bundle is now portable!")
    
    print("\nDone!")


if __name__ == '__main__':
    main()

