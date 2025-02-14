# libPT6964

A simple C++ library for interacting with the `PT6964` chip, which can manage a segmented display, made especially to be used on a Raspberry Pi.

For now, this library *probably* only works on my use case. Its functionality will be extended as soon as possible.

On a Pi, it uses the `pigpio` interface to communicate with the chip as fast as possible so that there's little to no latency when using it.

On other systems, you can make your own interface class by extending the BaseInterface class.

More documentation, fixes and examples are to be expected soon.