declare -a array=( 'mcf_s' 'lbm_s' 'wrf_s' 'cactuBSSN_s' 'pop2_s' 'cam4_s' )

filename="Zcache_Benchmarks.log"

echo "" > filename
echo "" >> filename
echo "" >> filename
echo "----------------------------------" >> filename

for benchmark in "${array[@]}"
do
    echo "----------------------------------" >> filename
    echo $benchmark >> filename
    echo "----------------------------------" >> filename
    ./build/ECE565-X86/gem5.fast configs/spec/spec_se.py -b $benchmark  -F=200000000 --maxinsts=700000000 --l2cache --caches --cpu-type=TimingSimpleCPU  --l1d_size=32kB --l1i_size=32kB --l2_size=6MB --l1d_assoc=8 --l1i_assoc=8 --l2_assoc=24
    grep "l2.overallMissRate::total" >> filename
    grep "system.cpu.exec_context.thread_0.numInsts" >> filename
    grep "system.cpu.numCycles" >> filename
    echo "----------------------------------" >> filename
    echo "" >> filename
    echo "" >> filename
    echo "" >> filename
    echo "" >> filename
done



# ./build/ECE565-X86/gem5.opt configs/spec/spec_se.py -b  --maxinsts=700000000 --l2cache --caches --cpu-type=TimingSimpleCPU -F=200000000 --l1d_size=32kB --l1i_size=32kB --l2_size=6MB --l1d_assoc=8 --l1i_assoc=8 --l2_assoc=24



