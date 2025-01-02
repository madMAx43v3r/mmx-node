
## AFL

### Install

```
sudo apt install llvm clang ninja-build

git clone https://github.com/AFLplusplus/AFLplusplus
cd AFLplusplus
make distrib
sudo make install
```

### Build with AFL

```
export AFL_CC_COMPILER=LLVM
./make_devel.sh -DCMAKE_C_COMPILER=afl-cc -DCMAKE_CXX_COMPILER=afl-c++
```

### Execute

```
echo core | sudo tee /proc/sys/kernel/core_pattern
```

```
AFL_SKIP_CPUFREQ=1 afl-fuzz -a binary -i test/vm/AFL/inputs/ -o test/vm/AFL/outputs/ ./build/test/test_execute @@
```
