
# Turbo cam processing

* [Deepstream SDK 4.0.2](https://developer.nvidia.com/deepstream-sdk)
 You can run deepstream-test1 sample to check Deepstream installation is successful or not.

* [TensorRT 6.0 GA](https://developer.nvidia.com/tensorrt)

* [TensorRT OSS (release/6.0 branch)](https://github.com/NVIDIA/TensorRT/tree/release/6.0)
This repository depends on the TensorRT OSS plugins. Specifically, the FasterRCNN sample depends on the `cropAndResizePlugin` and `proposalPlugin`; the MaskRCNN sample depends on the `ProposalLayer_TRT`, `PyramidROIAlign_TRT`, `DetectionLayer_TRT` and `SpecialSlice_TRT`; the SSD sample depends on the `batchTilePlugin`. To use these plugins for the samples here, complile a new `libnvinfer_plugin.so*` and replace your system `libnvinfer_plugin.so*`.

 Please note that TensorRT OSS 6.0 branch supports cross compilation. You can also compile natively and replace the plugin,

 ```
 $ git clone -b release/6.0 https://github.com/nvidia/TensorRT  && cd  TensorRT
 $ git submodule update --init --recursive && export TRT_SOURCE=`pwd`
 $ cd $TRT_SOURCE
 $ mkdir -p build && cd build
 $ wget https://github.com/Kitware/CMake/releases/download/v3.13.5/cmake-3.13.5.tar.gz
 $ tar xvf cmake-3.13.5.tar.gz
 $ cd cmake-3.13.5/ && ./configure && make && sudo make install
 $ cd ..
 $ /usr/local/bin/cmake .. -DTRT_BIN_DIR=`pwd`/out
 $ make nvinfer_plugin -j$(nproc)
 ## The libnvinfer_plugin.so* will be available in the `pwd`/out folder.  Then replace the system lib with the newly built lib.
 $ sudo cp /usr/lib/aarch64-linux-gnu/libnvinfer_plugin.so.6.x.x    /usr/lib/aarch64-linux-gnu/libnvinfer_plugin.so.6.x.x.bak
 $ sudo cp `pwd`/out/libnvinfer_plugin.so.6.x.x    /usr/lib/aarch64-linux-gnu/libnvinfer_plugin.so.6.x.x
 ```

## Build
 * $ export DS_SRC_PATH="Your deepstream sdk source path".
 * $ cd nvdsinfer_customparser_frcnn_uff or nvdsinfer_customparser_ssd_uff or nvdsinfer_customparser_mrcnn_uff
 * $ make
 * $ cd ..
 * $ make


The MaskRCNN configuration file is `pgie_mrcnn_uff_config.txt`.

## Run the sample app
Make sure "deepstream-test1" sample can run before running this app.
Once we have built the app and finished the configuration, we can run the app, using the command mentioned below.

```bash
$ ./deepstream-test3-app <uri1> [uri2] ... [uriN]
e.g.
  $ ./deepstream-test3-app file:///home/ubuntu/video1.mp4 file:///home/ubuntu/video2.mp4
  $ ./deepstream-test3-app rtsp://127.0.0.1/video1 rtsp://127.0.0.1/video2
```
