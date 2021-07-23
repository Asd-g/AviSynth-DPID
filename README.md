# Description

[dpid](http://www.gcc.tu-darmstadt.de/home/proj/dpid/) is an algorithm that preserves visually important details in downscaled images and is especially suited for large downscaling factors.

It acts like a convolutional filter where input pixels contribute more to the output image the more their color deviates from their local neighborhood, which preserves visually important details.

This is [a port of the VapourSynth plugin dpid](https://github.com/WolframRhodium/VapourSynth-dpid).

# Usage

```
DPID (clip input, int target_width, int target_height, float "lambdaY", float "lambdaU", float "lambdaV", float "src_leftY", float "src_leftU", float "src_leftV", float "src_topY", float "src_topU", float "src_topV", int "cloc")
```

Avsresize required in order to use this function (DPID).

## Parameters:

- input\
    A clip to process. It must be in planar format.
    
- target_width\
    The width of the output.
    
- target_height\
    The height of the output.
    
- lambdaY, lambdaU, lambdaV\
    The power factor of range kernel.\
    It can be used to tune the amplification of the weights of pixels that represent detail—from a box filter over an emphasis of distinct pixels towards a selection of only the most distinct pixels.\
    Must be greater than 0.1.\
    Default: lambdaY = 1.0; lambdaU = lambdaV = lambdaY.
    
- src_leftY, src_leftU, src_leftV\
    Cropping of the left edge.\
    Default: 0.0.
    
- src_topY, src_topU, src_topV\
    Cropping of the top edge.\
    Default: 0.0.
    
- cloc\
    Chroma location.\
    -1: If frame properties are supported and frame property "_ChromaLocation" exists - "_ChromaLocation" value is used.\
    If frame properties aren't supported or there is no frame property "_ChromaLocation" - 0.\
    0: Left.\
    1: Center.\
    2: Top left.\
    3: Top.\
    4: Bottom left.\
    5: Bottom.\
    Default: -1.
    
---

```
DPIDraw (clip input, clip input2, float "lambdaY", float "lambdaU", float "lambdaV", float "src_leftY", float "src_leftU", float "src_leftV", float "src_topY", float "src_topU", float "src_topV", int "cloc", int "y", int "u", int "v")
```

## Parameters:

- input\
    A clip to process. It must be in planar format.
    
- input2\
    User-defined downsampled clip.\
    Must be of the same format and number of frames as `input`.
    
- lambdaY, lambdaU, lambdaV\
    The power factor of range kernel.\
    It can be used to tune the amplification of the weights of pixels that represent detail—from a box filter over an emphasis of distinct pixels towards a selection of only the most distinct pixels.\
    Must be greater than 0.1.\
    Default: lambdaY = 1.0; lambdaU = lambdaV = lambdaY.
    
- src_leftY, src_leftU, src_leftV\
    Cropping of the left edge.\
    Default: 0.0.
    
- src_topY, src_topU, src_topV\
    Cropping of the top edge.\
    Default: 0.0.
    
- cloc\
    Chroma location.\
    -1: If frame properties are supported and frame property "_ChromaLocation" exists - "_ChromaLocation" value is used.\
    If frame properties aren't supported or there is no frame property "_ChromaLocation" - 0.\
    0: Left.\
    1: Center.\
    2: Top left.\
    3: Top.\
    4: Bottom left.\
    5: Bottom.\
    Default: -1.
    
- y, u, v\
    Planes to process.\
    1: Return garbage.\
    2: Copy plane from `input2`.\
    3: Process plane. Always process planes when clip is RGB.\
    Default: y = u = v = 3.
    
# Building

## Windows

Use solution files.

## Linux

### Requirements

- Git
- C++17 compiler
- CMake >= 3.16

```
git clone https://github.com/Asd-g/AviSynth-DPID && \
cd AviSynth-DPID && \
mkdir build && \
cd build && \
cmake .. && \
make -j$(nproc) && \
sudo make install
```
