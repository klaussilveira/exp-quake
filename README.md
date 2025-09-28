# cleanquake

cleanquake is a fork of [SDLQuake 1.0.9](https://www.libsdl.org/projects/quake/).
It has been stripped down and modernized to serve as a clean base for anyone
interested in creating new Quake source ports.

* Builds and runs on 64-bit systems
* Updated to use SDL2
* Removed unused headers and functions
* Less cruft

The idea is to remove as much as possible to make learning and tinkering more
approachable.

## Building

cleanquake uses SDL2 development libraries. On Debian:

    $ apt install libsdl2-dev

Then just run make:

    $ make

You can use the same command-line arguments as the original Quake,
such as `-fullscreen`, `-width`, `-height`, or `-nosound`.
