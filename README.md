# Fixposition Utilities and SDK

This is a collection of utilities that may be useful to use with Fixposition products.
The SDK part of this repo refers to C++ libraries and apps.


---
## Utilities

- [Software Update](software_update/README.md)
- [Recording utilities](record/README.md)


---
## SDK

[![CI](https://github.com/fixposition/fixposition_utility/actions/workflows/ci.yml/badge.svg)](https://github.com/fixposition/fixposition_utility/actions/workflows/ci.yml)

### Overview

- [fpcommon](fpcommon/README.md) c++ library with common functions
- [fpros1](fpros1/README.md) c++ library with common ROS functions
- [fpapps](fpapps/README.md) various apps

### Dependencies

- **Linux**, GCC, glibc, cmake, bash, ... (tested with Ubuntu 20.04)
- ROS (tested with [noetic-ros-base](https://hub.docker.com/_/ros/))
    - Note that some parts can be built without ROS. However, in this case some functionality will be unavailable
      in the built libraries and apps.
- yaml-cpp        (≥ ?,      tested with 0.6.2)
- boost           (≥ ?,      tested with 0.71.0)
- zlib1g          (≥ ?,      tested with 1.2.11)
- Eigen3          (≥ ?,      tested with 3.3.7)
- GTest           (≥ 1.12.0, tested with 1.13.0)
- PROJ            (≥ 9.0.0,  tested with 9.4.1)
- Cap'n Proto     (≥ 1.0.2,  tested with 1.0.2)

### Building

> *tl;dr*:
>
> ```sh
> docker pull ghcr.io/fixposition/fixposition/sdk:ci  # or: ./docker.sh build
> ./docker.sh run bash
> source /opt/ros/noetic/setup.bash
> make install INSTALL_PREFIX=fpsdk
> ./fpsdk/bin/fpltool
> ```

1. Setup build system, install dependencies

    The exact steps required depend on your system. You'll need the dependencies mentioned above installed system wide
    or otherwise tell CMake where to find them.

    Refer to the [Dockerfile](./Dockerfile) on installing the required dependencies on a Ubuntu 20.04.

2. Source ROS environment (optional, but required for some functionality)

    ```sh
    source /opt/ros/noetic/setup.bash
    ```

3. Configure

    ```sh
    cmake -B build -DCMAKE_INSTALL_PREFIX=~/fpsdk
    ```

    Additional parameters include: `-DCMAKE_BUILD_TYPE=Debug`, `-DROS_PACKAGE_PATH=/path/to/ros`, `-DBUILD_TESTING=OFF`

4. Build

    ```sh
    cmake --build build
    ```

5. Install

    ```sh
    cmake --install build
    ```

6. Enjoy!

    For example:

    ```sh
    ~/fpsdk/bin/fpltool -h
    ```

Alternatively, you can do `catkin build fpapps` in your ROS workspace.

The packages can be built individually. See the instructions in the [fpcommon](fpcommon/README.md),
[fpros1](fpros1/README.md) and [fpapps](fpapps/README.md) packages.

Refer to the CI workflow configuration ([ci.yaml](./.github/workflows/ci.yml)) for more options, such as different build
configurations or running tests.

---
## License

Unless otherwise noted, the content of this project is licensed under the MIT License.
See the [LICENSE](LICENSE) file for details.
