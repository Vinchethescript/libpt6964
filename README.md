# libPT6964

A small C++ library for communicating with the `PT6964` LED driver IC, made especially to be used on a Raspberry Pi <=4.
Though it seems stable at the time of writing, please note that this library is still under development. If you want to contribute, feel free to open an issue or a pull request. \

Code examples are available in the `examples` folder.
You may want to refer to the official [datasheet](https://cdn.instructables.com/ORIG/FFL/MQ98/IOEKA8HD/FFLMQ98IOEKA8HD.pdf) of the PT6964 chip for more information about its features.

On a Pi<=4, you can use the `pigpio` interface to communicate with the chip (requires the `pigpio` library and root privileges).
More interfaces may be added in the future. \
You can also make your own interface by extending the `pt6964::BaseInterface` class.

Better documentation is to be expected when possible.