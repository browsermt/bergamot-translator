#!/bin/bash

# Uses the command from https://stackoverflow.com/a/9355840/4565794.
# -v displays the commands executed to run compilation. Of this cc1 additional
#    flags which contain the flags triggered by -march=native is what we need.
# -E stop after preprocessing stage.

# Output on a linux machine with gcc-8 looks as follows:

# $ gcc -march=native -E -v - </dev/null 2>&1 | grep cc1
#       /usr/lib/gcc/x86_64-linux-gnu/8/cc1 -E -quiet -v -imultiarch x86_64-linux-gnu
#       - -march=skylake-avx512 -mmmx -mno-3dnow -msse -msse2 -msse3 -mssse3
#       -mno-sse4a -mcx16 -msahf -mmovbe -maes -mno-sha -mpclmul -mpopcnt -mabm
#       -mno-lwp -mfma -mno-fma4 -mno-xop -mbmi -mno-sgx -mbmi2 -mno-pconfig
#       -mno-wbnoinvd -mno-tbm -mavx -mavx2 -msse4.2 -msse4.1 -mlzcnt -mno-rtm
#       -mno-hle -mrdrnd -mf16c -mfsgsbase -mrdseed -mprfchw -madx -mfxsr -mxsave
#       -mxsaveopt -mavx512f -mno-avx512er -mavx512cd -mno-avx512pf -mno-prefetchwt1
#       -mclflushopt -mxsavec -mxsaves -mavx512dq -mavx512bw -mavx512vl
#       -mno-avx512ifma -mno-avx512vbmi -mno-avx5124fmaps -mno-avx5124vnniw -mclwb
#       -mno-mwaitx -mno-clzero -mpku -mno-rdpid -mno-gfni -mno-shstk
#       -mno-avx512vbmi2 -mavx512vnni -mno-vaes -mno-vpclmulqdq -mno-avx512bitalg
#       -mno-movdiri -mno-movdir64b --param l1-cache-size=32 --param
#       l1-cache-line-size=64 --param l2-cache-size=28160 -mtune=skylake-avx512
#       -fstack-protector-strong -Wformat -Wformat-security

# The sha256sum of the output is computed, and stripped to the first 8
# characters for use in ccache and github cache store key. Can effectively be
# considered as a hash of the compiler version and the flags activated by
# -march=native.

COMPILER=$1

$COMPILER -march=native -E -v - < /dev/null 2>&1 | grep cc1 \
    | sha256sum | cut -c1-8

