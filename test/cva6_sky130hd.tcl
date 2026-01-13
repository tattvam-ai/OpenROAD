# cva6 flow pipe cleaner
source "helpers.tcl"
source "flow_helpers.tcl"
source "sky130hd/cva6_sky130hd.vars"

set synth_verilog "cva6_sky130hd.v"
set design "cva6"
set top_module "cva6"
set sdc_file "cva6_sky130hd.sdc"
set die_area {0 0 3000 3000}
set core_area {200 200 2800 2800}

include -echo "cva6_flow.tcl"
