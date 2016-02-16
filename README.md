# Dynamixel Analyzer for Saleae Logic

This plugin for [Saleae Logic][logic] allows you to decode Robotis Dynamixel Servos

This analyzer was built with the current versions of the SDK (1.1.32) for the current released betas (1.1.25).   It has both support for showing the packets in the main area as well as in the decoded protocols section. 


* Contributor: Renée
* Contributor: Kurt
* Contributor: Mark Garrison (Saleae)

# Warning

This is a WIP and there are no guarantees of any kind.  Use at your own risk.

# Exporting

CSV export: WIP

Example output

```csv
Time [s],Type, ID, Errors, Reg start, count, data
0.000189666666667,0x03,0xFE,,0x03,0x01,0x01
0.000449666666667,0x01,0x01
0.000669666666667,0x06,0x02
0.000889666666667,0x03,0x01,,0x1E,0x02,0x90,0x01
0.001169666666667,0x00,0x01,,,0x00
0.001389666666667,0x83,0xFE,,0x18,0x01
0.001519750000000,0xFF,0x08,,0x18,0x01
0.001559750000000,0xFF,0x0E,,0x18,0x01
0.001599750000000,0xFF,0x02,,0x18,0x01
0.001639750000000,0xFF,0x07,,0x18,0x01
0.001679750000000,0xFF,0x0D,,0x18,0x01
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