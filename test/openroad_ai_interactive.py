import subprocess
import sys

# Paths (adjust relative to where this script is run)
synth_verilog = "gcd_sky130hd.v"
prompt_file = "../../sdc_gens/SiliTiming/prompts/prompt.txt"
sdc_file = "../../sdc_gens/SiliTiming/sdcs/AI_gen_gcd_sky130hd.sdc"

# AI SDC generation command
ai_command = "python3 ../../sdc_gens/SiliTiming/agent.py {} {} {}".format(
    synth_verilog, sdc_file, prompt_file
)

# Tcl commands to feed OpenROAD
tcl_commands = (
    "# Source helpers and vars\n"
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
    "# Generate SDC via AI\n"
    "puts \"Generating SDC via AI...\"\n"
    "if {[catch {exec " + ai_command + "} result]} {\n"
    "    puts \"Error generating SDC: $result\"\n"
    "    exit 1\n"
    "}\n\n"
    "# Continue with normal flow\n"
    "include -echo \"flow.tcl\"\n"
    "exit\n"
)

# Start OpenROAD subprocess
proc = subprocess.Popen(
    ["openroad"],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,  # merge stdout and stderr
    text=True,
    bufsize=1
)

# Stream output line by line
for line in tcl_commands.splitlines():
    proc.stdin.write(line + "\n")
proc.stdin.close()

# Print OpenROAD output in real time
for line in proc.stdout:
    print(line, end='')

proc.wait()
