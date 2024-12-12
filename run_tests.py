from multiprocessing import Pool
import subprocess
import os
from tqdm import tqdm
from datetime import datetime

OUTDIR="out"
SPECDIR="/home/timmy/spec2017"
MAXPROC = 56

# benchmarks will be 0 indexed
bench = {
    "blender_r": f"{SPECDIR}/blender_r/exe/blender_r_base.spectre_safebet-m64", # works
    # "cactuBSSN_r": f"{SPECDIR}/cactuBSSN_r/exe/cactusBSSN_r_base.spectre_safebet-m64", # boring
    # "cactuBSSN_s": f"{SPECDIR}/cactuBSSN_s/exe/cactusBSSN_s_base.spectre_safebet-m64", # boring
    "deepsjeng_r.test": (f"{SPECDIR}/deepsjeng_r/exe/deepsjeng_r_base.spectre_safebet-m64", "/home/timmy/spec2017/deepsjeng_r/data/test/input/test.txt"),
    "deepsjeng_s.test": (f"{SPECDIR}/deepsjeng_s/exe/deepsjeng_s_base.spectre_safebet-m64", "/home/timmy/spec2017/deepsjeng_r/data/test/input/test.txt"),
    "deepsjeng_r.train": (f"{SPECDIR}/deepsjeng_r/exe/deepsjeng_r_base.spectre_safebet-m64", "/home/timmy/spec2017/deepsjeng_r/data/train/input/train.txt"),
    "deepsjeng_s.train": (f"{SPECDIR}/deepsjeng_s/exe/deepsjeng_s_base.spectre_safebet-m64", "/home/timmy/spec2017/deepsjeng_r/data/train/input/train.txt"),
    "lbm_r.test": (f"{SPECDIR}/lbm_r/exe/lbm_r_base.spectre_safebet-m64", "20;reference.dat;0;1;/home/timmy/spec2017/lbm_s/data/test/input/200_200_260_ldc.of"),
    "lbm_s.test": (f"{SPECDIR}/lbm_s/exe/lbm_s_base.spectre_safebet-m64", "20;reference.dat;0;1;/home/timmy/spec2017/lbm_s/data/test/input/200_200_260_ldc.of"),
    "lbm_r.train": (f"{SPECDIR}/lbm_r/exe/lbm_r_base.spectre_safebet-m64", "200;reference.dat;0;1;/home/timmy/spec2017/lbm_s/data/test/input/200_200_260_ldc.of"),
    "lbm_s.train": (f"{SPECDIR}/lbm_s/exe/lbm_s_base.spectre_safebet-m64", "200;reference.dat;0;1;/home/timmy/spec2017/lbm_s/data/test/input/200_200_260_ldc.of"),
    # "leela_r": f"{SPECDIR}/leela_r/exe/leela_r_base.spectre_safebet-m64", # boring
    # "leela_s": f"{SPECDIR}/leela_s/exe/leela_s_base.spectre_safebet-m64", # boring
    # "mcf_r": f"{SPECDIR}/mcf_r/exe/mcf_r_base.spectre_safebet-m64", # boring
    # "mcf_s": f"{SPECDIR}/mcf_s/exe/mcf_s_base.spectre_safebet-m64", # boring
    "povray_r": f"{SPECDIR}/povray_r/exe/povray_r_base.spectre_safebet-m64", # works
}

# bench = { "helloworld": "gem5/tests/test-progs/hello/bin/x86/linux/hello" }

# l2size = [64, 128, 256, 512, 1024, 2048]
# num_sc = [2,4,6,8,12]
# l2assoc = [8, 16]
# l1size = [8, 16, 32, 64, 128]
# processor = [simple, 03, wide]

# best num_sc = 8

option_pkgs = [
    # {"l2_size": 128, "l2_assoc": 8, "rpl": "lru" }

    *[
        {"l1d_size": l1sz, "l2_size": l2sz, "l2_assoc": assoc, "rpl": "lru" } 
        for l1sz in [16, 64] 
        for l2sz in [64, 128, 256, 512, 1024, 2048] 
        for assoc in [8, 16]],
    
    *[
        {"l1d_size": l1sz, "l2_size": l2sz, "l2_assoc": 16, "rpl": "sc", "num_sc_blocks": n } 
        # for l1sz in [8, 16, 32, 64, 128] 
        for l1sz in [16, 64] 
        for n in [2,4,6,8,12]
        # for l2sz in [64, 128, 256, 512,]
        for l2sz in [64, 128, 256, 512, 1024, 2048]
    ]
]

OUTDIR="out"
commands = []

def try_to_run(command):
    try:
        result = subprocess.run(command, shell=True, capture_output=True, text=True)
        print(f"[{datetime.now().strftime('%H:%M:%S')}]: {'\033[32mSUCCESS\033[0m' if result.returncode == 0 else '\033[31mFAILURE\033[0m'} -- {command}")
            
        return {"command": command, "stdout": result.stdout, "stderr": result.stderr, "returncode": result.returncode}
    except Exception as e:
        return {"command": command, "error": str(e)}

if not os.path.exists(OUTDIR): os.makedirs(OUTDIR)
for b in bench:
    if not os.path.exists(outdir:=os.path.join(OUTDIR, b)): os.makedirs(outdir)

# assemble command line processes
for options in option_pkgs:
    ocmd = [f"--{k}={options[k]}" for k in options]
    ohdr = ".".join([f"{k}={options[k]}" for k in options])
    for i,b in enumerate(bench):
        cmd_args = ""
        binary = ""
        if isinstance(bench[b], tuple):
            binary = bench[b][0]
            cmd_args = bench[b][1]
        else:
            binary = bench[b]
        commands.append(" ".join([
            "gem5/build/X86/gem5.opt", 
            f"--outdir={OUTDIR}/{b}/{ohdr}",
            "./configs/main.py",
            f"--binary={binary}",
            f"--cmd_args=\"{cmd_args}\"",
            *ocmd,
            # "-F 10000000" # --fast-forward
            ]))

print(f"running {len(commands)} benchmarks")

with Pool(processes=MAXPROC if MAXPROC < len(commands) else len(commands)) as pool:
    results = pool.map(try_to_run, commands)

successes = 0
failures = []
with open("run.log", "w") as f:
    for result in results:
        f.write("+" + "-" * 79 + "\n")
        f.write(f"Command: {result['command']}\n")

        if "error" in result:
            f.write(f"Error: {result['error']}")
        else:
            if result['returncode'] != 0:
                failures.append(result)
            else:
                successes += 1
            f.write(f"Return Code: {result['returncode']}\n")
            f.write(f"Stdout:\n{result['stdout']}\n")
            f.write(f"Stderr:\n{result['stderr']}\n")
            
        f.write("+" + "-" * 79 + "\n\n\n")
    f.close()
    
with open("run.rpt", "w") as f:
    print("simulations completed.")
    print(f"{successes} successes. {len(failures)} failures.")
    f.write("simulations completed.\n")
    f.write(f"{successes} successes. {len(failures)} failures.\n\n")
    f.write(f"Failing runs:")
    for fail in failures:
        f.write("# " + "-"*78 + "\n")
        f.write(f"\nreturn code: {fail['returncode']}\n")
        f.write(f"\ncommand: {fail['command']}\n")
        f.write(f"\nstdout: {fail['stdout']}\n")
        f.write(f"\nstderr: {fail['stderr']}\n\n")
    f.close()

