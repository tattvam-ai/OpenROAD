#!/usr/bin/env python3
"""
SiliTiming CLI
Usage:
    python3 SiliTiming.py --sdc -design gcd
    ./SiliTiming.py --sdc -design gcd
"""

import subprocess
import sys
import time
import argparse
from pathlib import Path


parser = argparse.ArgumentParser(description="SiliTiming CLI frontend for OpenROAD + AI SDC generation")
parser.add_argument("--sdc", action="store_true", help="Generate SDC via AI agent")
parser.add_argument("-design", type=str, required=True, help="Design name (e.g., gcd)")
args = parser.parse_args()

design_name = args.design
generate_sdc = args.sdc

# -------------------------------
# File paths (adjust relative to this script)
# -------------------------------
# base_dir = Path(__file__).parent.resolve()
# sdc_dir = base_dir / "../../sdc_gens/SiliTiming/sdcs"
# prompt_dir = base_dir / "../../sdc_gens/SiliTiming/prompts"

project_root = Path("/home/ubuntu/silipilot") 
sdc_dir = project_root / "sdc_gens/SiliTiming/sdcs"
prompt_dir = project_root / "sdc_gens/SiliTiming/prompts"

sdc_dir.mkdir(parents=True, exist_ok=True)

synth_verilog = f"{design_name}.v"
prompt_file = prompt_dir / "prompt.txt"
sdc_file = sdc_dir / f"AI_gen_{design_name}.sdc"

ai_command = f"python3 ../../sdc_gens/SiliTiming/agent.py {synth_verilog} {sdc_file} {prompt_file}"

def get_top_module(verilog_file):
    """
    Parse the Verilog file and return the first module name.
    """
    with open(verilog_file, 'r') as f:
        for line in f:
            line = line.strip()
            # Look for 'module <name> ...'
            if line.startswith("module "):
                # Split by space and take the second token
                parts = line.split()
                if len(parts) >= 2:
                    # Remove any trailing '(' or ';'
                    module_name = parts[1].split('(')[0].split(';')[0]
                    return module_name
    return None

top_module = get_top_module(synth_verilog)

if not top_module:
    print(f"Error: Could not identify top module in {synth_verilog}")
    sys.exit(1)

print(f"[INFO] Detected top module: {top_module}")

tcl_commands = [
    "# Welcome to SiliTiming CLI",
    "",
    "# Reading helpers and vars...",
    "source helpers.tcl",
    "source flow_helpers.tcl",
    "source sky130hd/sky130hd.vars",
    "",
    "# Set design variables",
    f"set synth_verilog \"{synth_verilog}\"",
    f"set design \"{design_name}\"",
    f"set top_module \"{top_module}\"",
    f"set sdc_file \"{sdc_file}\"",
    "",
    "# Set die and core area",
    "set die_area {0 0 299.96 300.128}",
    "set core_area {9.996 10.08 289.964 290.048}",
    ""
]

if generate_sdc:
    print(f"[AI] Running SDC agent: {ai_command}")
    result = subprocess.run(ai_command, shell=True)
    if result.returncode != 0:
        print("[AI] Error: SDC generation failed")
        sys.exit(1)
    print("[AI] SDC generation completed!")

# Then build TCL commands without exec
tcl_commands = [
    "# Continue with OpenROAD flow",
    f"set sdc_file \"{sdc_file}\"",
    "include -echo \"flow.tcl\"",
    "puts \"OpenROAD flow completed.\"",
    "exit"
]

proc = subprocess.Popen(
    ["openroad"],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    text=True,
    bufsize=1
)


for line in tcl_commands:
    if line.strip() == "":
        print()
    else:
        # Highlight AI command
        if line.startswith("puts \"\\033[33m[AI]"):
            print(f"\033[33m{line}\033[0m")
        else:
            print(line)
    proc.stdin.write(line + "\n")
    proc.stdin.flush()
    time.sleep(0.75)  

proc.stdin.close()


for out_line in proc.stdout:
    print(out_line, end='')

proc.wait()
