

# ServerDemo
The demo server  integrate with components like [libevent](https://github.com/libevent/libevent) / [protobuf](https://github.com/google/protobuf) / [gflags](https://github.com/gflags/gflags) / [glog](https://github.com/google/glog) .

# Compiling the source code with CMake
1. Get the source files.
2. Install third party modules
3. Create build directory and change to it.
4. Run CMake to configure the build tree.
5. Build the software using selected build tool.
6. Run or Install the built files.

On Unix-like systems with GNU Make as build tool, these build steps can be summarized by the following sequence of commands executed in a shell, where $package and $version are shell variables which represent the name of this package and the obtained version of the software.

```
$ git clone https://github.com/taozuhong/ServerDemo.git
$ cd ServerDemo

$ echo "install libevent / gflags / glog"
$ sudo yum install libevent libevent-devel gflags gflags-devel glog glog-devel

$ echo "install protobuf 2.6.1"
$ git clone -b v2.6.1 https://github.com/google/protobuf.git
$ cd protobuf
$ make & sudo make install & sudo ldconfig

$ mkdir ServerDemo/build && cd ServerDemo
$ cmake CMakeLists.txt

  - Press 'c' to configure the build system and 'e' to ignore warnings.
  - Set CMAKE_INSTALL_PREFIX and other CMake variables and options.
  - Continue pressing 'c' until the option 'g' is available.
  - Then press 'g' to generate the configuration files for GNU Make.

$ make
$ make install (optional)
```
