  * OS environment preparation
  
```
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
```

  * Project initialization
  
```
git submodule init
git submodule update
cd firmware/rpi-zero/pico-sdk
git submodule update --init
```

s