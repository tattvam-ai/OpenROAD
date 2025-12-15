# # gcd flow pipe cleaner
# source "helpers.tcl"
# source "flow_helpers.tcl"
# source "sky130hd/sky130hd.vars"

# set synth_verilog "gcd_sky130hd.v"
# set design "gcd"
# set top_module "gcd"
# set sdc_file "gcd_sky130hd.sdc"
# set die_area {0 0 299.96 300.128}
# set core_area {9.996 10.08 289.964 290.048}

# include -echo "flow.tcl"

# gcd flow pipe cleaner
source "helpers.tcl"
source "flow_helpers.tcl"
source "sky130hd/sky130hd.vars"

set synth_verilog "gcd_sky130hd.v"
set design "gcd"
set top_module "gcd"
set sdc_file "../../sdc_gens/SiliTiming/sdcs/AI_gen_gcd_sky130hd.sdc"      
set prompt_file "../../sdc_gens/SiliTiming/prompts/prompt.txt"
set die_area {0 0 299.96 300.128}
set core_area {9.996 10.08 289.964 290.048}

# Generate SDC via Gemini AI
if {[catch {exec python3 ../../sdc_gens/SiliTiming/agent.py \
                   $synth_verilog \
                   $sdc_file \
                   $prompt_file} result]} {
    puts "Error generating SDC: $result"
    exit 1
}

include -echo "flow.tcl"