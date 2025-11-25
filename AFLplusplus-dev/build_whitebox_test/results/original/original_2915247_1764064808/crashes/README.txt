Command line used to find this crash:

/home/apulis-dev/code/AFLplusplus-dev/afl-fuzz -i /home/apulis-dev/code/AFLplusplus-dev/build_whitebox_test/testcases -o /home/apulis-dev/code/AFLplusplus-dev/build_whitebox_test/results/original -S original_2915247_1764064808 -m none -t 1000 -- /home/apulis-dev/code/AFLplusplus-dev/build_whitebox_test/test-whiteBox

If you can't reproduce a bug outside of afl-fuzz, be sure to set the same
memory limit. The limit used for this fuzzing session was 0 B.

Need a tool to minimize test cases before investigating the crashes or sending
them to a vendor? Check out the afl-tmin that comes with the fuzzer!

Found any cool bugs in open-source tools using afl-fuzz? If yes, please post
to https://github.com/AFLplusplus/AFLplusplus/issues/286 once the issues
 are fixed :)

