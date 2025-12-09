#!/usr/bin/env python3
import argparse
import os
import platform
import subprocess
import shutil
import sys

def run_command(command, cwd=None, shell=False):
    """Run a shell command and print output."""
    print(f"Running: {' '.join(command) if isinstance(command, list) else command}")
    try:
        subprocess.check_call(command, cwd=cwd, shell=shell)
    except subprocess.CalledProcessError as e:
        print(f"Error running command: {e}")
        sys.exit(1)

def setup_dependency(name, repo_url, install_dir, build_type, cmake_args, workspace_dir):
    """Clone, build, and install a dependency."""
    print(f"=== Setting up {name} ===")
    
    # checkout
    repo_dir = os.path.join(workspace_dir, name)
    if not os.path.exists(repo_dir):
        run_command(["git", "clone", repo_url, name], cwd=workspace_dir)
    
    # build dir
    build_dir = os.path.join(repo_dir, "build")
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
    os.makedirs(build_dir)
    
    # configure
    # Filter out empty arguments
    cmd_cmake = ["cmake", "-B", ".", "-S", ".."] + [arg for arg in cmake_args if arg]
    
    # Add generator if Ninja is available and not specified (optional, but good for CI)
    # Using default generator usually works fine if standard environment.
    # On Windows CI often uses VS generator by default or Ninja.
    # The workflows use Ninja on Linux/macOS and VS on Windows usually, 
    # but sometimes Ninja on Windows. Let's respect what might be passed or default.
    
    run_command(cmd_cmake, cwd=build_dir)
    
    # build
    run_command(["cmake", "--build", ".", "--config", build_type, "--parallel"], cwd=build_dir)
    
    # install
    run_command(["cmake", "--install", ".", "--config", build_type], cwd=build_dir)
    
    print(f"=== {name} setup complete ===\n")

def patch_thread_system_config(install_dir, is_windows):
    """Patch thread_system-config.cmake to remove fmt dependency."""
    print("Patching thread_system-config.cmake...")
    
    # Find the config file
    config_file = None
    for root, dirs, files in os.walk(install_dir):
        if "thread_system-config.cmake" in files:
            config_file = os.path.join(root, "thread_system-config.cmake")
            break
            
    if not config_file:
        print("Warning: thread_system-config.cmake not found")
        return

    print(f"Found config file: {config_file}")
    
    with open(config_file, 'r') as f:
        content = f.read()
        
    # Replace dependency call
    # find_dependency(fmt) -> # find_dependency(fmt)
    # Regex replacement might be safer but simple replace handles the exact string usually generated.
    # The workflows use sed with regex: 'find_dependency[[:space:]]*(fmt[^)]*)'
    import re
    new_content = re.sub(r'find_dependency\s*\(\s*fmt[^)]*\)', '# \g<0>', content)
    
    with open(config_file, 'w') as f:
        f.write(new_content)
        
    print("Patched config file successfully.")

def main():
    parser = argparse.ArgumentParser(description="Build and install dependencies")
    parser.add_argument("--build-type", default="Release", help="Build type (Debug/Release)")
    parser.add_argument("--install-prefix", required=True, help="Base install directory")
    parser.add_argument("--compiler", default="", help="Compiler type (gcc/clang/msvc)")
    parser.add_argument("--os", default=platform.system(), help="OS type (Linux/Darwin/Windows)")
    
    args = parser.parse_args()
    
    workspace_dir = os.getcwd() # Assumes running from root of repo where deps are checked out sibling or sub
    # Actually workflows check out deps into subfolders of workspace.
    
    # Common CMake args
    base_cmake_args = [
        f"-DCMAKE_BUILD_TYPE={args.build_type}",
        "-DBUILD_TESTS=OFF",
        "-DBUILD_SAMPLES=OFF",
        "-DUSE_STD_FORMAT=ON" # We enforce this for all
    ]
    
    # OS/Compiler specific
    if args.os == "Linux" and args.compiler == "gcc":
        base_cmake_args.extend([
            "-DCMAKE_C_COMPILER=gcc-13",
            "-DCMAKE_CXX_COMPILER=g++-13"
        ])
    elif args.os == "Windows":
        # Windows specific hints if needed, usually defaults work with VS generator
        pass
        
    install_base = args.install_prefix
    
    # 1. common_system
    common_install = os.path.join(install_base, "common_system_install")
    common_args = base_cmake_args + [
        f"-DCMAKE_INSTALL_PREFIX={common_install}"
    ]
    setup_dependency("common_system", "https://github.com/kcenon/common_system.git", common_install, args.build_type, common_args, workspace_dir)
    
    # 2. thread_system
    thread_install = os.path.join(install_base, "thread_system_install")
    thread_args = base_cmake_args + [
        f"-DCMAKE_INSTALL_PREFIX={thread_install}",
        f"-DCMAKE_PREFIX_PATH={common_install}",
        f"-DCMAKE_CXX_FLAGS=-I{common_install}/include" # Sometimes needed if not properly exported
    ]
    # Windows CMake flags syntax
    if args.os == "Windows":
         thread_args[-1] = f"-DCMAKE_CXX_FLAGS=/I{common_install}/include"
         
    setup_dependency("thread_system", "https://github.com/kcenon/thread_system.git", thread_install, args.build_type, thread_args, workspace_dir)
    
    # Patch thread_system
    patch_thread_system_config(thread_install, args.os == "Windows")
    
    # 3. logger_system
    logger_install = os.path.join(install_base, "logger_system_install")
    logger_args = base_cmake_args + [
        f"-DCMAKE_INSTALL_PREFIX={logger_install}",
        f"-DCMAKE_PREFIX_PATH={common_install};{thread_install}",
    ]
    
    # CXX flags for includes
    cxx_flags = f"-I{common_install}/include -I{thread_install}/include"
    if args.os == "Windows":
        cxx_flags = f"/I{common_install}/include /I{thread_install}/include"
    logger_args.append(f"-DCMAKE_CXX_FLAGS={cxx_flags}")

    setup_dependency("logger_system", "https://github.com/kcenon/logger_system.git", logger_install, args.build_type, logger_args, workspace_dir)

if __name__ == "__main__":
    main()
