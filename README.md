# sdrpp-mirisdr-source
Libmirisdr source plugin for SDR++

Binary installing:

Visit the Actions page, find latest commit build artifacts, download mirisdr_source.so and put it to /usr/lib/sdrpp/plugins/, skipping to the step 3. Don't forget to install latest libmirisdr-4!

Building:

  1.  Install SDR++ core headers to /usr/include/sdrpp_core/, if not installed. Refer to https://cropinghigh.github.io/sdrpp-moduledb/headerguide.html about how to do that

  OR if you don't want to use my header system, add -DSDRPP_MODULE_CMAKE="/path/to/sdrpp_build_dir/sdrpp_module.cmake" to cmake launch arguments

  Install libmirisdr-4(ubuntu repo version is very old, use debian repo or build from source)

  2.  Build:

          mkdir build
          cd build
          cmake ..
          make
          sudo make install

  3.  Enable new module by enabling it via Module manager

