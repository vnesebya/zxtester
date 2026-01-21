#How to build using pico-sdk

```bash
git clone -b master https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init
export PICO_SDK_PATH=$(pwd)
cd ..
git clone https://github.com/tony-mikhailov/zxtester
cd zxtester
mkdir build
cd ./build
cmake ..
make
```
