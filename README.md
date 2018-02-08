# Dynamixel Analyzer for Saleae Logic

This plugin for [Saleae Logic][logic] allows you to decode Robotis Dynamixel Servos

This analyzer was built with the current versions of the SDK (1.1.32) for the current released betas (1.1.25).   It has both support for showing the packets in the main area as well as in the decoded protocols section. 


* Contributor: Renée
* Contributor: Kurt
* Contributor: Mark Garrison (Saleae)

# Warning

This is a WIP and there are no guarantees of any kind.  Use at your own risk.

# Limitations

The currently released version only supports Protocol 1 packets. Up till now I have only owned AX-12 servos with maybe one or two AX-18 servos.

I am currently experimenting with the new XL430-W250 servos, so I started adding Protocol 2 support.
In this branch I added this Servo type to the types of servos.

The WIP version that supports Protocol 2 packets can be found in the WIP---Protocol2-support branch

https://github.com/KurtE/SaleaeDynamixelAnalyzer/tree/WIP---Protocol2-support

# Exporting

CSV export: WIP

Example output

```csv
Time [s],Instruction, Instructions(S), ID, Errors, Reg start, Reg(s), count, data
5.3178775625000,0x02,READ,0x01,,0x00,MODEL,0x0A
5.3181380000000,0x00,REPLY,0x01,,,,0x0A,0x0C,0x00,0x18,0x01,0x01,0x00,0x00,0x00,0xFF,0xFF
5.3258038750000,0x02,READ,0x01,,0x0A,DATA2,0x0A
5.3259348125000,0x00,REPLY,0x01,,,,0x0A,0x01,0x46,0x3C,0x8C,0xFF,0x03,0x02,0x24,0x24,0x00
5.3358213125000,0x02,READ,0x01,,0x14,DCAL,0x0A
5.3359455625000,0x00,REPLY,0x01,,,,0x0A,0x2E,0x00,0xC9,0x03,0x00,0x00,0x01,0x01,0x20,0x20
5.3457037500000,0x02,READ,0x01,,0x1E,GOAL,0x0A
5.3459314375000,0x00,REPLY,0x01,,,,0x0A,0x8F,0xFF,0xFF,0xFF,0xFF,0x03,0x8F,0x01,0x00,0x00
5.3557421875000,0x02,READ,0x01,,0x28,PLOAD,0x0A
5.3558661875000,0x00,REPLY,0x01,,,,0x0A,0x00,0x00,0x7B,0x21,0x00,0x00,0x00,0x00,0x20,0x00
5.3657606250000,0x02,READ,0x01,,0x2A,PVOLT,0x01
5.3658849375000,0x00,REPLY,0x01,,,,0x01,0x7B


```

# Building

Download the [Logic SDK][sdk] and extract it somewhere on your
machine. 

How you build depends on what platform.  There may be several ways to do it, but I will describe what I have done.  

## Windows
Clone the repo and cd into the top level of it:

    git clone https://github.com/KurtE/SaleaeDynamixelAnalyzer.git
    cd SaleaeDynamixelAnalyzer

Note: Actually I use github for windows here, but.... 

Copy the Link and Include directories of the SDK into the top level directory of where you installed this project. 

Load the project into Visual Studio (note, I am using VS 2015.  In here you should be able to choose if you wish to build for 32 or 64 bits likewise debug or release 

## OSX and Linux

Follow the instructions given in the SDK, to download (or copy) your project into the top level of where you installed the SDK. 

Run the build script:

    ./build_analyzer.py

Note: on Linux I had to edit the file ./build_analyzer.py and change the linker options to use -lAnalyzer64 instead of -lAnalyzer

## Installing

In the Developer tab in Logic preferences, specify the path for
loading new plugins, then copy the built plugin into that location:

    cp release/* /path/specified/in/Logic/preferences

[logic]: https://www.saleae.com/downloads
[sdk]: http://support.saleae.com/hc/en-us/articles/201104644-Analyzer-SDK

Note: when I am developing on this, I often put the directory of where I am building into the Saleae preferences and as such when I am making changes, I simply exit the Logic Program, do the build and then launch Logic again. 