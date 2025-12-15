import subprocess
import sys
import time  # <-- import time for delays

'''
Usage:
   python3 openroad_ai_interactive.py 
'''

synth_verilog = "gcd_sky130hd.v"
prompt_file = "../../sdc_gens/SiliTiming/prompts/prompt.txt"
sdc_file = "../../sdc_gens/SiliTiming/sdcs/AI_gen_gcd_sky130hd.sdc"

ai_command = "python3 ../../sdc_gens/SiliTiming/agent.py {} {} {}".format(
    synth_verilog, sdc_file, prompt_file
)

tcl_commands = (
    "# Welcome to SiliTiming. Interactive Mode Enabled\n\n"
    "# Thinking about the design and the constraints...\n\n"
    "# Reading the helpers and vars...\n"
    "source helpers.tcl\n"
    "source flow_helpers.tcl\n"
    "source sky130hd/sky130hd.vars\n\n"
    "# Set design variables\n"
    "set synth_verilog \"{}\"\n".format(synth_verilog) +
    "set design \"gcd\"\n"
    "set top_module \"gcd\"\n"
    "set sdc_file \"{}\"\n\n".format(sdc_file) +
    "# Set die and core area\n"
    "set die_area {0 0 299.96 300.128}\n"
    "set core_area {9.996 10.08 289.964 290.048}\n\n"
    "# Generate SDC via AI model....\n"
    "if {[catch {exec " + ai_command + "} result]} {\n"
    "    puts \"Error generating SDC: $result\"\n"
    "    exit 1\n"
    "}\n\n"
    "# Getting a formatted SDC from the AI model....\n"
    "# SDC File ready\n"
    "# Read SDC file into the OpenROAD flow for GCD sky130hd...\n"
    "# Executing OpenROAD flow\n"
    "include -echo \"flow.tcl\"\n"
    "# GCD OpenROAD Flow Completed.\n"
)

# Start OpenROAD subprocess
proc = subprocess.Popen(
    ["openroad"],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,  
    text=True,
    bufsize=1
)

# # Start OpenROAD subprocess
# proc = subprocess.Popen(
#     ["openroad"],
#     stdin=subprocess.PIPE,
#     stdout=subprocess.PIPE,
#     stderr=subprocess.STDOUT,  # merge stdout and stderr
#     text=True,
#     bufsize=1
# )

for line in tcl_commands.splitlines():
    proc.stdin.write(line + "\n")
    proc.stdin.flush()
    print(f"{line}") 
    time.sleep(0.75)  

proc.stdin.close()

for line in proc.stdout:
    print(line, end='')

proc.wait()

