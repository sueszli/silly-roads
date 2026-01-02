#!/usr/bin/env python3
import sys
import subprocess
import re

def main():
    if len(sys.argv) < 2:
        print("Usage: filter_leaks.py <command> [args...]")
        sys.exit(1)

    command = sys.argv[1:]

    # Leaks to suppress (regex patterns)
    suppressions = [
        r"NSArray.*CoreFoundation", # Caused by _glfwDestroyWindowCocoa
        r"AXObserverCookie.*HIServices", # Caused by _glfwPollEventsCocoa
    ]

    try:
        # Run the command and capture output
        # Using subprocess.PIPE to capturing stdout/stderr
        # We need to print the output in real-time or after execution, but filtering it.
        # Since 'leaks' usually runs at exit or on a pid, we might just want tocapture all output 
        # for analysis. 'leaks' command output is what we want to filter. 
        # However, the makefile runs: leaks ... -- ./binary
        # So we should run the command and capture its stdout.
        
        process = subprocess.run(
            command,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT 
        )

        output = process.stdout
        lines = output.split('\n')
        
        filtered_output = []
        skip_block = False
        
        # Simple state machine to parse `leaks` output blocks
        # A leak block starts with "Leak: ..." and contains a Call stack.
        
        current_leak = []
        
        for line in lines:
            if line.startswith("Leak: "):
                # Finish previous leak block
                if current_leak:
                    block_str = "\n".join(current_leak)
                    if not any(re.search(s, block_str) for s in suppressions):
                        filtered_output.append(block_str)
                    current_leak = []

                current_leak.append(line)
            elif line.strip().startswith("Call stack:") or (current_leak and line.strip()):
                 current_leak.append(line)
            elif not line.strip() and current_leak:
                 # End of block (empty line)
                current_leak.append(line)
                block_str = "\n".join(current_leak)
                if not any(re.search(s, block_str) for s in suppressions):
                    filtered_output.append(block_str)
                current_leak = []
            else:
                if not current_leak:
                    filtered_output.append(line)

        # Flush last leak if any
        if current_leak:
            block_str = "\n".join(current_leak)
            if not any(re.search(s, block_str) for s in suppressions):
                filtered_output.append(block_str)

        final_output = "\n".join(filtered_output)
        print(final_output)

        # Determine exit code: if we see "0 leaks" or only suppressed leaks, return 0. 
        # The `leaks` tool returns 1 if leaks are found.
        # Check if the filtered output still contains "Leak: "
        if "Leak: " in final_output:
             sys.exit(1)
        else:
             sys.exit(0)

    except KeyboardInterrupt:
        sys.exit(130)
    except Exception as e:
        print(f"Error running filter script: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
