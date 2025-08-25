cmake -S . -B build $@
cd build
make -j$(nproc)