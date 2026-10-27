# Separate project: does not alter the original Lab 2 project or reports.
proc validate_alternate {} {
    set root [file normalize [file dirname [info script]]]
    cd $root
    set stage all
    if {[info exists ::env(LAB3_VALIDATE_STAGE)]} { set stage $::env(LAB3_VALIDATE_STAGE) }
    if {$stage ne "all" && $stage ne "csim"} { error "Stage must be all or csim" }
    open_project hls_alt128_validation
    set_top ggml_vec_dot_q8_0_q8_0
    set flags "-std=c++11 -ffp-contract=off -I$root"
    add_files vec_dot_q8.cpp -cflags $flags
    add_files -tb test_extended.cpp -cflags $flags
    open_solution solution1 -flow_target vivado
    set_part {xczu3eg-sbva484-1-e}
    create_clock -period 10 -name default
    source directives.tcl
    file mkdir results
    csim_design -clean
    set csim_csv hls_alt128_validation/solution1/csim/build/extended_results.csv
    if {![file exists $csim_csv]} { error "C simulation did not generate the results CSV" }
    file copy -force $csim_csv results/extended_csim_results.csv
    if {$stage eq "csim"} {
        puts "C simulation complete; synthesis and RTL have NOT been run."
        return
    }
    csynth_design
    set synth hls_alt128_validation/solution1/syn/report
    file copy -force $synth/csynth.rpt results/csynth_summary.rpt
    file copy -force $synth/ggml_vec_dot_q8_0_q8_0_csynth.rpt results/alternate128_csynth.rpt
    foreach report [glob -nocomplain $synth/*Pipeline*csynth.rpt] {
        file copy -force $report [file join results [file tail $report]]
    }
    cosim_design -rtl verilog -tool xsim -trace_level none
    set cosim hls_alt128_validation/solution1/sim/report/ggml_vec_dot_q8_0_q8_0_cosim.rpt
    if {![file exists $cosim]} { error "RTL co-simulation report is missing" }
    file copy -force $cosim results/alternate128_cosim.rpt
    set rtl_csv hls_alt128_validation/solution1/sim/wrapc_pc/extended_results.csv
    if {[file exists $rtl_csv]} { file copy -force $rtl_csv results/extended_cosim_results.csv }
    puts "Validation flow finished. Inspect ALL VALIDATION TESTS PASSED, achieved block II, and Verilog Pass."
}
if {[catch {validate_alternate} message options]} {
    puts stderr "VALIDATION FLOW FAILED: $message"
    if {[dict exists $options -errorinfo]} { puts stderr [dict get $options -errorinfo] }
    exit 1
}
exit 0
