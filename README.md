# Aether

Aether is an algorithmic reverb linux LV2 plugin based off of [Cloudseed](https://github.com/ValdemarOrn/CloudSeed).

**Aether is early on in development and may be subject to backwards incompatible changes**

## Contents
* [Building](#Building)
	* [Dependencies](#dependencies)
	* [Compiling](#compiling)
	* [Additional Options](#additional-options)

## Building

### Dependencies

**Build Tools**

| Tool  | Version |
| ----- | ------- |
| CMake | >= 3.10 |

**Compiler**

| Compiler | Version |
| -------- | ------- |
| g++      | >=10.0  |
| clang++  | >=10.0  |

**Libraries**

| Library | Deb Package  |
| ------- | ------------ |
| Lv2     | `lv2-dev`    |
| X11     | `libx11-dev` |

**Ubuntu/Debian**
The prerequisites can be installed using apt via the command:
```bash
sudo apt install cmake g++-10 lv2-dev libx11-dev
```

### Compiling

First clone the repository using:
```bash
git clone --recurse-submodules -j4 https://github.com/Dougal-s/Aether.git
cd Aether
```
Then create the build directory and compile the plugin using:
```bash
mkdir build && cd build
cmake ..
make -j4
```

> Some systems may default to an older version of the compiler causing compilation errors. To fix this, clear the build directory and then run `cmake ..` with the `CXX` environment variable set to the newer compiler (e.g. `g++-10` or `clang++-10`)

### Additional Options

| Option      | Description | Values   |
| ----------- | ----------- | -------- |
| BUILD_TESTS | Build unit tests and benchmarks. The tests can be run using `make test` and individual tests can be found in `builds/tests`. | `on` / `off` |
| CMAKE_BUILD_TYPE | Debug adds runtime checks and debug information. Release enables additional optimizations. Can also be set using the `--config` flag when running cmake.  | `debug` / `release` |
