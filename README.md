# Quick Start

- EN7523 EVB Board

```bash
make clean
cp configs/en7523_evb_defconfig .config
make olddefconfig
make CROSS_COMPILE="arm-linux-gnueabi-" -j$(nproc)
```

- AN7581 EVB Board

```bash
make clean
cp configs/an7581_evb_defconfig .config
make olddefconfig
make CROSS_COMPILE="aarch64-linux-gnu-" -j$(nproc)
```
