# ZxTester

Идея проекта взята из проекта NanoLogic <https://github.com/vnesebya/nanologic.git> - простого логического пробника с функцией отрисовки форм сигнала похожей на осцилограмму
После экспериментов с Arduino Nano, стало ясно, что слабый микроконтроллер использующийся в Arduinо Nano не в состоянии решить поставленной задачи, после чего было принята идея попробовать реализовать прототип на других микроконтроллерах. Для этих целей был выбран RP2040-Zero.
Первые експерименты показали, что RP2040-Zero прекрасно справляется в качестве частотомера до 10Мгц и даже выше.
Тесты показали, что устройство легко считает и 14MHz, что для отладки ZX-SPECTRUM оказалось более чем достаточно.

## Настройка окружения для сборки кода

### Установка pico-sdk

```bash
sudo apt install cmake python3 build-essential gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib

git clone -b master https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init
export PICO_SDK_PATH=$(pwd)
cd ..
```

### Клонирование репозитория

```bash
git clone https://github.com/tony-mikhailov/zxtester
cd zxtester
```

### Сборка кода

```bash
mkdir -p build
cd ./build
cmake ..
make
```

### Готовые файлы прошивок

- [ztester.uf2](tree/master/uf2)
