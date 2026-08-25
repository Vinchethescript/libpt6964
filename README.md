# libPT6964

A small C++ library for communicating with the `PT6964` LED driver IC, made especially to be used on a Raspberry Pi <=4.
**This branch is currently untested and will probably not compile/work properly.** It reimplements the entire code as a header-only library and uses CMake for installing. Functionally, almost nothing should change, so upgrading should be easy enough. If you want to contribute, feel free to open an issue or a pull request.

This code restructure introduces performance and efficiency improvements by adding templates and better compile-time adjustments. 

Code examples are available in the `examples` folder.
You may want to refer to the official [datasheet](https://www.alldatasheet.com/datasheet-pdf/pdf/391723/PTC/PT6964-S.html) of the PT6964 chip for more information about its features.

On a Pi<=4, you can use the `pigpio` interface to communicate with the chip (requires the `pigpio` library and root privileges).
More interfaces, including a Pi 5-supported one, may be added in the future. \
You can also make your own interface by implementing the `HardwareInterface` concept.

Better documentation is to be expected when possible.