Max external that works with YARP libraries.

See yarp.maxhelp for details. example.maxpat has shows another demo application.

expects ACE_ROOT and YARP_DIR to be defined;
expects YARP libs to be compiled as static libraries

(you'll see hard references to
"/Users/johnty/Documents/yarp-2.3.22/build/lib/Release/libYARP_XXX.a"
which will have to be pointed to your deployment of yarp.


Running:

(windows) expects ACE(d).dll dynamic library in runtime path of Max/Msp, next to the Max executable
(osx) expects libACE.dylib to be in /usr/lib (or somewhere similar)



compiled and tested with:
- VS2012, 32bit. on Win 7. Max 5 and 6.1.4 SDKs
- OSX 10.9. XCode 5.x universal. Max 5 and 6 SDKs
