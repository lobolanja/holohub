"""
Process management utilities for end-to-end tests.

Provides helper functions to start, monitor, and manage subprocess execution
for multi-process integration testing.
"""

import subprocess
import sys
import threading
from typing import List, Union


class CompletedProcess:
    """Wrapper for subprocess results with stdout/stderr."""
    
    def __init__(self, returncode: int, stdout: str, stderr: str):
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr


def start_process(cmd: Union[str, List[str]]) -> subprocess.Popen:
    """
    Start a subprocess without waiting for completion.
    
    Args:
        cmd: Command to execute (string or list of arguments)
        
    Returns:
        Popen object with stdout/stderr pipes
    """
    if isinstance(cmd, str):
        cmd_args = cmd.split()
    else:
        cmd_args = cmd
    
    print(f"[process_utils] Starting process: {' '.join(cmd_args)}")
    
    process = subprocess.Popen(
        cmd_args,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )
    
    return process


def monitor_process(process: subprocess.Popen, timeout: int = None) -> CompletedProcess:
    """
    Monitor a process until completion, streaming output in real-time.
    
    Args:
        process: Popen object to monitor
        timeout: Maximum time in seconds to wait for process completion (None = no timeout)
        
    Returns:
        CompletedProcess with return code and captured output
        
    Raises:
        subprocess.TimeoutExpired: If process doesn't complete within timeout
    """
    stdout_lines = []
    stderr_lines = []
    
    def read_stdout():
        for line in process.stdout:
            print(line, end='')
            stdout_lines.append(line)
    
    def read_stderr():
        for line in process.stderr:
            print(line, end='', file=sys.stderr)
            stderr_lines.append(line)
    
    # Start threads to read stdout and stderr
    stdout_thread = threading.Thread(target=read_stdout, daemon=True)
    stderr_thread = threading.Thread(target=read_stderr, daemon=True)
    
    stdout_thread.start()
    stderr_thread.start()
    
    # Wait for process to complete with optional timeout
    try:
        returncode = process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        print(f"\n[process_utils] Process timed out after {timeout}s - killing process")
        process.kill()
        # Give threads a moment to finish reading
        stdout_thread.join(timeout=2)
        stderr_thread.join(timeout=2)
        
        stdout_text = ''.join(stdout_lines)
        stderr_text = ''.join(stderr_lines)
        
        # Re-raise with captured output for debugging
        raise subprocess.TimeoutExpired(
            cmd=process.args,
            timeout=timeout,
            output=stdout_text,
            stderr=stderr_text
        )
    
    # Wait for output threads to finish
    stdout_thread.join(timeout=5)
    stderr_thread.join(timeout=5)
    
    stdout_text = ''.join(stdout_lines)
    stderr_text = ''.join(stderr_lines)
    
    result = CompletedProcess(returncode, stdout_text, stderr_text)
    
    if returncode != 0:
        # Don't raise immediately - let caller decide if non-zero is an error
        # (e.g., timeout returns 124 which might be expected)
        pass
    
    return result


def run_command(cmd: Union[str, List[str]], stream_output: bool = False) -> CompletedProcess:
    """
    Run a command synchronously.
    
    Args:
        cmd: Command to execute (string or list of arguments)
        stream_output: If True, stream output in real-time
        
    Returns:
        CompletedProcess with return code and captured output
    """
    if stream_output:
        process = start_process(cmd)
        return monitor_process(process)
    else:
        if isinstance(cmd, str):
            cmd_args = cmd.split()
        else:
            cmd_args = cmd
        
        result = subprocess.run(
            cmd_args,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        
        return CompletedProcess(result.returncode, result.stdout, result.stderr)
