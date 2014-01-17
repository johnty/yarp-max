very rough sandbox version of a max external that works with YARP libraries.

expects ACE_ROOT and YARP_DIR to be defined;
expects YARP libs to be compiled as static libraries
expects ACE(d).dll dynamic library in runtime path of Max/Msp

most likely won't work "out of the box" - this is just a backup version

currently only registers ports; doesn't do much else.

xcode project not tested - will need the lib dependencies to be added (and most likely other things as well)!

compiled with VS2012, 32bit.
