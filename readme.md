![alt text](ReaKontrol_v100.png?raw=true)

# ReaKontrol
- Fork of the brumbear@pacificpeaks and it's from the excellent ReaKontrol repository originally published by James Teh: https://github.com/jcsteh/reaKontrol
- License: GNU General Public License version 2.0.
- License Notes: As the original work is published under GPLv2 the modified programs are also licensed under GPLv2. May be updated to GPLv3 if copyright holder of original work agrees to update too.

## Building
This section is for those interested in building ReaKontrol from source code.
The build instructions have changed from the parent repository, this section will reflect these changes,
cmake is now used for better cross platform maintenance and syntax highlighting + support in IDEs (and less dependencies).

### Dependencies
To build ReaKontrol, you will need:

- CMake, version 3.12 or later:
    * [Download CMake](https://cmake.org/download/)

#### Specific Windows dependencies

- Microsoft Visual Studio 2017 Community:
    * Visual Studio 2019 is not yet supported.
    * [Download Visual Studio 2017 Community](https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=Community&rel=15)
    * When installing Visual Studio, you need to enable the following:
        - On the Workloads tab, in the Windows group: Desktop development with C++

#### Specific OSX dependencies

- Command Line Tools for Xcode

### How to build on Windows with VS 2017
To build ReaKontrol, from a command prompt, change to the ReaKontrol checkout directory and run the following commands:
```
cd build
cmake .. -A x64
cmake --build . --config Release
```
The resulting dll can be found in the `build/bin/Release` directory (Windows)
Note: If you are still building for Reaper x86 32bit specify -A x86 as target architecture above.

### How to build on OSX
#### Intel x86 based Mac
To build ReaKontrol, from a command prompt, change to the ReaKontrol checkout directory and run the following commands:
```
cd build
cmake .. 
cmake --build . --config Release
```
The resulting dylib can be found in the `build/lib` directory (OSX).

#### ARM based Mac
First, comment out 'set(CMAKE_OSX_ARCHITECTURES x86_64)' in 'CMakeLists.txt'. Then build as described above.

Both ARM and x86 binaries can reside in parallel and load depending on Rosetta settings by naming them like this:

```
ls ~/Library/Application\ Support/REAPER/UserPlugins

reaper_kontrol-x86_64.dylib
reaper_kontrol.dylib
```
### How to Install
If you have followed the build steps, you can attach the last command:
```
cmake --build . --target install
```
The rea_kontrol.dll is then copied to your
- Win32: %APPDATA%/Reaper/UserPlugins
- OSX: $home/Library/Application Support/Reaper/UserPlugins
 folder.
