Slow Scan Television Decoder for Raspberry Pi Pico and Seeed Wio Terminal
=========================================================================

.. image::  images/sstv_decoder_thumbnail.png
  :target: https://youtu.be/FraeQk6Fj2I


Features
--------

+ Cheap, Easy Build
+ Minimal External Components
+ Martin, Scottie, Robot and PD modes
+ Works with any SSB receiver using headphone connector or directly by mic (wio terminal)
+ Standalone Design (no PC or soundcard needed)


Getting the Code
----------------

.. code::

  git clone https://github.com/dawsonjon/PicoSSTV.git


Documentation
-------------

For technical details refer to the `technical documentation <https://101-things.readthedocs.io/en/latest/sstv_decoder.html>`__.

3D-Printed Enclosure
--------------------

A 3D printed enclosuer can be found `here <https://github.com/dawsonjon/PicoSSTV.git>`__, including stl files and FreeCAD design files.


Install Arduino Pico
--------------------

The SSTV code is written in pure C++, but a demo application is provided as an `Arduino sketch <https://github.com/dawsonjon/PicoSSTV/tree/main/sstv_decoder>`__. The `Arduino Pico <https://github.com/earlephilhower/arduino-pico>`__ port by Earle Philhower is probably the easiest way to install and configure a C++ development environment for the Raspberry Pi Pico. Its possible to install the tool and get up-and running with example applications in just a few minutes. Refer to the `installation instructions <https://github.com/earlephilhower/arduino-pico?tab=readme-ov-file#installing-via-arduino-boards-manager>`__ and the `online documentation <https://arduino-pico.readthedocs.io/en/latest/>`__ to get started.

Install Seeed Wio Terminal
--------------------------
Once you've installed everything you need to use the Wio Terminal on the Arduino IDE, copy the sstv_library folder to the libraries folder.
Open and load on the wio the examples/sstv_receive_wio sketch.

.. image::  images/sstv_decoder_thumbnail_wio.png

Credits
-------

This project uses the ILI934X display driver by Darren Horrocks.

