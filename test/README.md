
## AFL

### Install

```
sudo apt install llvm clang gcc-10-plugin-dev ninja-build

git clone https://github.com/AFLplusplus/AFLplusplus
cd AFLplusplus
make distrib
sudo make install
```

### Build with AFL

`./make_devel.sh -DCMAKE_C_COMPILER=afl-clang-fast -DCMAKE_CXX_COMPILER=afl-clang-fast++`

### Execute

`echo core | sudo tee /proc/sys/kernel/core_pattern`

`afl-fuzz -i test/vm/AFL/inputs/ -o test/vm/AFL/ouputs/ ./build/test/test_execute @@`
