Cookit
========================

## Description

Cookit is a Tcl/Tk runtime environment similar to tclkit with a focus on a balance between functionality and executable size. It allows using Tcl/Tk in both console mode and graphical mode to run Tcl scripts, as well as packaging applications into a single executable without external dependencies.

Cookit is a single executable file that contains:

- Tcl/Tk version **8.6.15** (with Threads enabled) or **9.0.0**
- Statically linked packages: **cookfs**, **tclvfs**, **Threads**, **tclmtls**, **tdom**, **twapi** (for Windows platform)
- Other packages: **tkcon**

Supported platforms:

- Linux x86 / x86_64
- Windows x86 / x86_64
- macOS x86_64

This means that Cookit can be easily and simply used to develop both console and GUI applications, which can be multi-threaded, send HTTPS requests to third-party services, process the received JSON/XML response with **tdom**. For debugging in GUI mode a convenient and uniform on all platforms console **tkcon** is available.  After development, the application can be packaged into a single executable file without dependencies and used in other environments as a standalone application.

It can also be used as a replacement for tclsh/wish.

At the same time, the executable file has minimal size.

- for Linux platform: executable file without Tk - about 1.1MB, executable file with Tk - about 1.7MB
- for Windows platform: executable without Tk - about 1.5MB, executable with Tk - about 2MB.

This is an amazing size considering the ability to create GUI applications with support for SSL/TLS connections, work with JSON/XML documents, extensive access to WinAPI using **twapi** on Windows platform. In normal installations, only the size of the OpenSSL library will be 2 times larger.

As a use case, consider an internal installer that works in both console and GUI mode and contains the same code for all platforms. This installer uses the REST GitHub API via HTTPS to get information about the latest available release, uses tdom to parse the JSON response, downloads a platform-appropriate tar.gz archive from GitHub releases using HTTPS, mounts the resulting tar.gz archive using tclvfs and extracts the necessary files to the destination directory.

## Installation

### Initial installation

To simplify installation, a minimal installer is available that can be run with a single command.

Linux x86_64:
```shell
curl -sL https://github.com/chpock/cookit/releases/latest/download/cookit-installer.x86_64-pc-linux-gnu.sh | sh
```

Linux x86:
```shell
curl -sL https://github.com/chpock/cookit/releases/latest/download/cookit-installer.i686-pc-linux-gnu.sh | sh
```

For Windows platform it is necessary to download and run the installation file:

- Windowx x86_64: [https://github.com/chpock/cookit/releases/latest/download/cookit-installer.x86_64-w64-mingw32.exe](https://github.com/chpock/cookit/releases/latest/download/cookit-installer.x86_64-w64-mingw32.exe)
- Windowx x86: [https://github.com/chpock/cookit/releases/latest/download/cookit-installer.i686-w64-mingw32.exe](https://github.com/chpock/cookit/releases/latest/download/cookit-installer.i686-w64-mingw32.exe)

Note: The GUI installation does not show any messages during the installation process, only a dialog box after the installation process is finished. Therefore, you may get the impression that there is no reaction when the file is started. Please wait 20-30 seconds after launching the installation file.

Also, the Windows installer adds the `%USERPROFILE%\.cookit` directory to the `PATH` for the current user.

This will download the latest available Cookit [releases](https://github.com/chpock/cookit/releases) on GitHub.

After successful installation, the Cookit binary files will be available in the home directory `~/.cookit` for unix platforms or `%USERPROFILE%\.cookit` for Windows.

### Upgrading

To upgrade, simply run on Unix platforms:
```shell
$ ~/.cookit/cookit --upgrade
```

On Windows platforms:
```
PS> %USERPROFILE%\.cookit\cookit.exe --upgrade
```

This will check for [releases](https://github.com/chpock/cookit/releases) on GitHub. If there is a newer version, it will be downloaded and installed.

### Uninstalling

To uninstall, simply run on Unix platforms:
```shell
$ ~/.cookit/cookit --uninstall
```

On Windows platforms:
```
%USERPROFILE%\.cookit/cookit.exe --uninstall
```

This will delete all files related to Cookit.

## Available Cookit executable files

Cookit consists of standalone and independent executables. Depending on the task, the appropriate engine can be chosen.

The contents of the files are slightly different on Unix and Windows platforms, but a common, intuitive scheme is used.

The following engines are available for Unix platforms:

- **cookit** is a build without Tk
- **cookit-gui** is a build with Tk. By default, it runs in console mode. Tk will become available after package requires Tk. This makes it possible to create universal applications that can run in both console mode and GUI mode.
- files with the `*U`  suffix (**cookitU** and **cookitU-gui**) are the same builds, but do not use the [UPX](https://github.com/upx/upx/tree/devel) executable archiver

More engines are available for the Windows platform:

- **cookit.exe** is a build without Tk
- **cookit-gui.com** is a build with Tk, but runs as a console application by default. Tk becomes available after package requires Tk.
- **cookit-gui.exe** is an build with Tk, but runs as a GUI application by default.
- files with the `*A`  suffix (**cookitA.exe**, **cookitA-gui.com**, **cookitA-gui.exe**) are the same builds as above, but with an administrative manifest. These builds will ask for confirmation of privilege elevation at startup. They can be used for applications that require elevated privileges.
- files with the `*U`  suffix (**cookitU.exe**, **cookitU-gui.com**, **cookitU-gui.exe**) are the same builds, but do not use the [UPX](https://github.com/upx/upx/tree/devel) executable archiver.
- files with the `*UA`  suffix (**cookitUA.exe**, **cookitUA-gui.com**, **cookitUA-gui.exe**) are builds with an administrative manifest that do not use the [UPX](https://github.com/upx/upx/tree/devel) executable archiver.

For all these variants, engines with prefix `*8` are also available. These are engines with **Tcl 8.6**. Those engines without this prefix are with **Tcl 9.0**.

## Usage

### As a replacement for tclsh / wish

Cookit can be used as a complete replacement for **tclsh** / **wish** to run scripts.

code reading from stdin is supported:
```shell
$ echo 'puts "Hello World"' | ~/.cookit/cookit
Hello World
```

running script files:
```shell
$ cat hello.tcl
puts "Hello World"
$ ~/.cookit/cookit hello.tcl
Hello World
```

If launched without parameters, the interactive interpreter will be started. Usual run commands files, such as `.tclshrc` or `.wishrc` (or their usual aliases on the Windows platform), will be processed before showing the command prompt.

### Command line parameters

If Cookit is started with a first argument that starts with two dashes (`--`) then this is treated as an internal command.

The following internal commands are supported:

- **--version** - shows the Cookit version
- **--install** - checks for the latest available Cookit [release](https://github.com/chpock/cookit/releases) on GitHub and installs it to the ~/.cookit  directory
- **--check-upgrade** - checks if there is a newer available Cookit [release](https://github.com/chpock/cookit/releases) on GitHub and displays the result of this check without taking any action
- **--upgrade** - checks for a newer available Cookit [release](https://github.com/chpock/cookit/releases) on GitHub and updates the current installation if the last available release is newer than the current one
- **--uninstall** - uninstalls all Cookit files
- **--wrap** - creates standalone and independent applications from a set of files, this command is described in more detail in the corresponding section
- **--stats** - shows statistics and composition of a standalone application built with **--wrap**

### Creating a standalone application

The **--wrap** command is used to create a standalone application. It allows to package the Tcl script, packages/libraries and any additional data into a single executable file.

The full command format is as follows:
```shell
$ cookit --wrap <main script> ?optional_arguments?
```

The `<main script>` parameter specifies the name of the script that will be run by default when the executable is started. Inside the executable it will be named `main.tcl`

The minimum format of this command is in the form of:
```shell
$ cookit --wrap <some script>
```

In this case, an executable file will be created from a single script. The output file name format will be taken from the script file name, but with the `.tcl` extension removed.

For example:
```shell
$ cat hello.tcl
puts "Hello World"
$ ~/.cookit/cookit --wrap hello.tcl
$ ./hello
Hello World
```

For more complex applications, the following optional arguments for the **--wrap** command are supported:

- **--paths**, **--path**, **--to**, **--as** - allow to flexibly control the set of files that will be in the resulting executable file. These options are described in detail below.
- **--output <file name>** - specifies the name of the output executable file. By default, Cookit tries to determine the output file name from the <main script>  file name.
- **--stubfile <cookit file path>** - specifies the Cookit used for the output executable. For example, if you specify a Cookit for the Windows platform, then the output file will be for that platform. Or, for example, you are building in console mode, but the output file should be a GUI application (with Tk), then you need to specify with this parameter the Cookit with Tk enabled.
- **--compression <compression method>:<compression level>** - allows to specify compression method and compression level. Currently supported compression methods are `zlib` and `lzma` , as well as uncompressed format `none`.
- **--icon <icon file path>** - (Windows only) allows to set an icon for the executable file.
- **--company <value>**, **--copyright <value>**, **--fileversion <value>**, **--productname <value>**, **--productversion <value>**, **--filedescription <value>**, **--originalfilename <value>** - (Windows only) allows to set version info for the output executable file for Windows platform.

### File manipulation in the resulting executable file

To flexibly control the contents of the resulting executable file, the following options are used in sequence:

- **--paths**, **--path** - to specify files/directories in the local file system
- **--to**, **--as** - to specify the location for files/directories that were specified by the preceding **--paths** or **--path** options

The format for these options is as follows:

- **--paths <file...>** - specifies one or more files/directories in the local file system
- **--path <file>** - points to a single file/directory in the local file system
- **--to <relative_directory>** - specifies the directory in the resulting executable in which the files/directories specified by the previous **--paths** or **--path** options will be placed.
- **--as <relative_file>** - specifies the full file name under which the file/directory specified by the previous **--paths** or **--path** options will be placed.

Practical example:

It is necessary to:

- include packages from the `lib` directory, which is in the current directory, as well as packages from the `/opt/tcl/packages` directory
- add the data files from `/opt/my_data_ver1` as `data`
- add data index file from `/opt/index_ver1.idx` as `data/index.idx`
- add additional Tcl scripts `parse.tcl` and `help.tcl` from the current directory `generic` to the root of the virtual file system
- add additional Tcl script `/opt/support_ver1.tcl` to the root of the virtual file system as `support.tcl`
- use the `app.tcl` file in the current directory as the main script to be executed at startup.

```shell
cookit --wrap app.tcl \
    --paths ./lib/* --paths /opt/tcl/packages/* --to ./lib \
    --path /opt/my_data_ver1 --as ./data \
    --path /opt/index_ver1.idx --as ./data/index.idx \
    --paths ./generic/parse.tcl ./generic/help.tcl --to . \
    --path /opt/support_ver1.tcl --as ./support.tcl
```

## Copyrights

Copyright (c) 2024 Konstantin Kushnir <chpock@gmail.com>

## License

`cookit` sources are available under the same license as [Tcl core](https://tcl.tk/software/tcltk/license.html).
