#MediaTime

## Deps

 * libyuv
 * libx264
 * libx265
 * live555
 * librtmp


## Install depends

```bash
cd third/libyuv
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=./../../../dist/ ..
make -j16
make install
```

```bash
cd third/x264
./configure --prefix=../../dist/ --enable-shared --enable-static --disable-avs  --disable-swscale --disable-lavf --disable-ffms --disable-gpac  --disable-lsmash --disable-asm
make -j16
make install
```

## Build application
```bash
make
```

## Run application
```bash
LD_LIBRARY_PATH=./dist/lib/ ./MediaTime
```

## Others
