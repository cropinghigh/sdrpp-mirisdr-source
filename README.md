# sdrpp-mirisdr-source
Libmirisdr source plugin for SDR++

Building:

  1.  Install SDR++ core headers to /usr/include/sdrpp_core/, if not installed. Refer to sdrpp-headers-git AUR package PKGBUILD on instructions how to do that

  OR if you don't want to use my header system, add -DSDRPP_MODULE_CMAKE="/path/to/sdrpp_build_dir/sdrpp_module.cmake" to cmake launch arguments

  2.  Build:

          mkdir build
          cd build
          cmake ..
          make
          sudo make install

  4.  Enable new module by enabling it via Module manager

