# Building Guide

### Software needed
* [Git](https://git-scm.com/download)
* [Android Studio](https://developer.android.com/studio) – We recommend to get the latest stable version

### Steps
<details><summary><i>Windows only</i></summary>
<p>

> In a terminal prompt with **administrator** privileges, enable git symlinks globally:
>
> ```cmd
> git config --global core.symlinks true
> ```
>
> Use this elevated prompt for the next steps.

</p>
</details>

Clone the repo **recursively**, either with your preferred Git GUI or with the command below:
```cmd
git clone https://github.com/skyline-emu/skyline.git --recursive
```

Open Android Studio
<details><summary><i>First time users only</i></summary>
<p>

> If you opened Android Studio for the first time, choose the `Standard` install type and complete the setup wizard leaving all settings to their default value.
> <p><img height="400" src="https://user-images.githubusercontent.com/37104290/162196602-4c142ed0-0c26-4628-8062-7ac9785201cc.png"></p>
> If you get any errors on "Intel® HAXM" or "Android Emulator Hypervisor Driver for AMD Processors", you can safely ignore them as they won't be used for Skyline.

</p>
</details>

Import the project by clicking on the `Open` icon, then in the file picker choose the `skyline` folder you just cloned in the steps above:
<p>
    <img height="400" src="https://user-images.githubusercontent.com/37104290/162200497-dddfa9f0-00c6-4a32-84c2-1f0ff743a7e2.png"> 
    <img height="400" src="https://user-images.githubusercontent.com/37104290/162196879-08d9684b-c6a2-4636-9c23-c026cb7d7494.png">
</p>

Exclude the following folders from indexing:
- `app/libraries/llvm`
- `app/libraries/boost`

To exclude a folder, switch to the project view:
<p>
    <img height="400" src="https://user-images.githubusercontent.com/37104290/163343887-56a0b170-2249-45c4-a758-2b33cbfbc4ab.png"> 
    <img height="400" src="https://user-images.githubusercontent.com/37104290/163343932-bed0c59c-7aaa-44f6-bdc4-0a10f966fb56.png">
</p>

In the project view navigate to the `app/libraries` folder, right-click on the folder you want to exclude and navigate the menus to the `Exclude` option:
<p>
    <img height="400" src="https://user-images.githubusercontent.com/37104290/162200274-f739e960-82ca-4b12-95eb-caa88a063d61.png"> 
    <img height="400" src="https://user-images.githubusercontent.com/37104290/162196999-a0376e13-0399-4352-a30d-85d6785151a9.png">
</p>

If an `Invalid Gradle JDK configuration found` error comes up, select `Use Embedded JDK`:
<p><img height="250" src="https://user-images.githubusercontent.com/37104290/162197215-b28ea3ec-ed5c-4d83-ac9a-19e892caa129.png"></p>

An error about NDK not being configured will be shown:
<p><img height="250" src="https://user-images.githubusercontent.com/37104290/162197226-3d9bf980-19af-4cad-86a3-c43cad05e185.png"></p>

Ignore the suggested version reported by Android Studio. Instead, find the NDK version to download inside the `app/build.gradle` file:
```gradle
ndkVersion 'X.Y.Z'
```
From that same file also note down the CMake version required:
```gradle
externalNativeBuild {
    cmake {
        version 'A.B.C+'
        path "CMakeLists.txt"
    }
}
```

Open the SDK manager from the top-right toolbar:
<p><img height="75" src="https://user-images.githubusercontent.com/37104290/162198029-4f29c50c-75eb-49ce-b05f-bb6413bf844c.png"></p>

Navigate to the `SDK Tools` tab and enable the `Show Package Details` checkbox in the bottom-right corner:
<p><img height="400" src="https://user-images.githubusercontent.com/37104290/162198766-37045d58-352c-48d6-b8de-67c1ddb7c757.png"></p>

Expand the `NDK (Side by side)` and `CMake` entries, select the appropriate version from the previous step, then click `OK`.

Finally, sync the project:
<p><img height="75" src="https://user-images.githubusercontent.com/37104290/162199780-b5406b5d-480d-4371-9dc4-5cfc6d655746.png"></p>


## Common issues (and how to fix them)

* `Cmake Error: CMake was unable to find a build program corresponding to "Ninja"`

Check that you installed the correct CMake version in the Android Studio SDK Manager. If you're sure you have the correct one, you may try adding the path to CMake binaries installed by Android Studio to the `local.properties` file:
```properties
cmake.dir=<path-to-cmake-folder>
```
E.g. on Windows:
```properties
cmake.dir=C\:\\Users\\skyline\\AppData\\Local\\Android\\Sdk\\cmake\\3.18.1
```

* `'shader_compiler/*.h' file not found`

You didn't clone the repository with symlinks enabled. Windows requires administrator privileges to create symlinks so it's likely it didn't create them.
In an **administrator** terminal prompt navigate to the Skyline root project folder and run:
```cmd
git submodule deinit app/libraries/shader-compiler
git config core.symlinks true
git submodule foreach git config core.symlinks true
git submodule update --init --recursive app/libraries/shader-compiler
```
If you'd like to, you can enable symlinks globally by running: (this will only affect new repositories)
```cmd
git config --global core.symlinks true
```
