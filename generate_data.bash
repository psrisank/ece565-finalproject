#!/bin/bash

benches=("mcf_s" "lbm_s" "cactuBSSN_s" "pop2_s" "cam4_s")

filename="Zcache_Benchmarks.log"


rm $filename

for benchmark in "${benches[@]}"; do
    echo "----------------------------------" >> $filename
    echo $benchmark >> $filename
    echo "----------------------------------" >> $filename

    ./build/ECE565-X86/gem5.fast configs/spec/spec_se.py -b $benchmark  -F=200000000 --maxinsts=700000000 --l2cache --caches --cpu-type=TimingSimpleCPU  --l1d_size=24kB --l1i_size=32kB --l2_size=512kB --l1d_assoc=6 --l1i_assoc=8 --l2_assoc=8
    wait
    grep "l2.overallMissRate::total" m5out/stats.txt >> $filename
    grep "numInsts" m5out/stats.txt >> $filename
    grep "numCycles" m5out/stats.txt >> $filename
    echo "----------------------------------" >> $filename
    echo "" >> $filename
    echo "" >> $filename
    echo "" >> $filename
    echo "" >> $filename
done



# ./build/ECE565-X86/gem5.opt configs/spec/spec_se.py -b  --maxinsts=700000000 --l2cache --caches --cpu-type=TimingSimpleCPU -F=200000000 --l1d_size=32kB --l1i_size=32kB --l2_size=6MB --l1d_assoc=8 --l1i_assoc=8 --l2_assoc=24



# for i in {1..5}; do
#     ./build/ECE565-X86/gem5.fast configs/spec/spec_se.py -b mcf_s  -F 2000 --maxinsts=4000 --l2cache --caches --cpu-type=TimingSimpleCPU  --l1d_size=32kB --l1i_size=32kB --l2_size=6MB --l1d_assoc=8 --l1i_assoc=8 --l2_assoc=24
# done
