# Aether

[![Automated builds](https://github.com/Dougal-s/Aether/workflows/Automated%20builds/badge.svg?branch=master&event=push)](https://github.com/Dougal-s/Aether/actions/workflows/build.yml?query=branch%3Amaster)

![screenshot of the Aether UI](screenshot.png)

Aether is an algorithmic reverb LV2 plugin based on [Cloudseed](https://github.com/ValdemarOrn/CloudSeed).

For a quick overview of the user interface and controls, please refer to the [user manual](usermanual/USERMANUAL.md).

For a more technical overview of the plugin architecture, please refer to:
https://github.com/ValdemarOrn/CloudSeed/tree/master/Documentation

## Contents
* [Downloads](#Downloads)
* [Building](#Building)
	* [Dependencies](#dependencies)
	* [Compiling](#compiling)
	* [Additional Options](#additional-options)
	* [Installing](#installing)

## Downloads
Binaries from the latest release can be found under [releases](https://github.com/Dougal-s/Aether/releases), while binaries from the master branch can be found under the [actions tab](https://github.com/Dougal-s/Aether/actions/workflows/build.yml?query=branch%3Amaster).

## Building

### Dependencies

**Build Tools**

| Tool  | Version |
| ----- | ------- |
| CMake | >= 3.10 |

**Compiler**

| Compiler | Version |
| -------- | ------- |
| g++      | >=9.0  |
| clang++  | >=10.0  |
| AppleClang  | >=12.0  |
| msvc     | >=19.29 |

**Libraries**

| Library | Deb Package  |
| ------- | ------------ |
| X11     | `libx11-dev` |
| OpenGL  | `libgl1-mesa-dev` `libglu1-mesa-dev` |

**Ubuntu/Debian**
```bash
sudo apt install cmake g++-10 libx11-dev libgl1-mesa-dev libglu1-mesa-dev
```

**MacOS**
* Should work as it is.

**Windows**
* Install Visual Studio Community from: https://visualstudio.microsoft.com/vs/community/
* Install CMake from: https://cmake.org/download/
* Install Git from: https://git-scm.com/download/win

### Compiling

First clone the repository and create the build directory using:
```bash
git clone --recurse-submodules -j4 https://github.com/Dougal-s/Aether.git
cd Aether
mkdir build && cd build
```

Then compile the plugin using:

**Linux/MacOS**
```bash
cmake .. -DCMAKE_BUILD_TYPE="Release"
make -j4
```
> Some systems may default to an older version of the compiler causing compilation errors. To fix this, clear the build directory and then run `cmake ..` with the `CXX` environment variable set to the newer compiler (e.g. `g++-10` or `clang++-10`)

**Windows**
```bash
cmake -G "Visual Studio 16 2019" ..
cmake --build . --config=release -j4
```

### Additional Options

| Option      | Description | Values   |
| ----------- | ----------- | -------- |
| BUILD_GUI | Build gui.  | `on` / `off` |
| BUILD_TESTS | Build unit tests. The tests can be run using `make test` and individual tests can be found in `builds/tests/tests`. | `on` / `off` |
| BUILD_BENCHMARKS | Build benchmarks. The benchmarks can be run using `make test` and individual benchmarks can be found in `builds/tests/benchmarks`. | `on` / `off` |
| CMAKE_BUILD_TYPE | Debug adds runtime checks and debug information. Release enables additional optimizations. Can also be set using the `--config` flag when running cmake.  | `debug` / `release` |
| FORCE_DISABLE_DENORMALS | Disables denormal floating point numbers at the beginning of every processing block. This is usually redundant as the plugin host should already do this. Defaults to `on`. | `on` / `off` |

### Installing

The build process will create an lv2 plugin bundle called `aether.lv2` in the build directory, which can be copied to the appropriate [platform specific location](https://lv2plug.in/pages/filesystem-hierarchy-standard.html)(`~/.lv2`, `$HOME/Library/Audio/Plug-Ins/LV2`, `%APPDATA%/LV2`), where it can be picked up by an lv2 plugin host.
