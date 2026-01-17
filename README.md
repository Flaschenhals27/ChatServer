Welcome to the chat project
===========================

This is the project skeleton for the chat server you are going to implement in this course.
The following notes will help you getting started.

Cloning the repository
======================

In case you forgot how to get a local copy of the repository from the server to your development machine, here is
a little reminder:

1. Log in to the [Gitlab Server](https://gitlab.rwu.de).
2. Navigate to your chat server project, e.g. `stud-syspr/2025-ws/g01/chat`.
3. In the menu below the project description, choose whether you want to use HTTPS or SSH.
4. Copy the repository address to the clipboard using the corresponding button.
5. Open a terminal and change to the directory where you want your local repository to be stored.
   Remember that git will create a subdirectory with the name of the project.
6. Use `git clone`, e.g.: `git clone git@gitlab.rwu.de:stud-syspr/2025-ws/g01/chat.git`

Using CMake to build the project
================================

You will use CMake to build the project, so you do not have to write your own Makefile.
Even better, you do not even have to deal a lot with CMake yourself, as the
[required `CMakeLists.txt` control file](CMakeLists.txt) is already included.

To build the project, change to the build subdirectory, execute `cmake` and then `make`:
```
cd chat/build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```
In the lines above, the Debug build type was selected, so you can use `gdb` or another debugger during development.

If you decide to introduce additional modules, you can add them to `SERVER_MODULES` in
[`CMakeLists.txt`](CMakeLists.txt).

Description of the modules
==========================

You will see several modules in the [`src/` subdirectory of the project](src/).
Some of them already contain a little bit of code and comments giving you some hints about what to do.

You will find some functions setting `errno` to `ENOSYS` and returning an error.
This is just a convention for functions that are not yet implemented, making them fail in a defined way instead of
doing something random.
**When you have implemented such a function, it should no longer fail with `ENOSYS`!**

Each module has a given purpose as described below.

`broadcastagent`
----------------

Here you are going to implement the broadcast agent.
You will not need this module in the first steps, only later on in the course.

`clientthread`
--------------

Here you implement the thread function for the client thread, heavily using the functions provided by the
`network` and `user` modules.

`connectionhandler`
-------------------

This is where you call `accept()` for incoming client connections and use the `user` module to add the client to
your user list.
Of course, to make this work, you will have to create the server socket first.

`main`
------

Pretty obvious, isn't it? Here you evaluate the command line arguments and initialize the other modules.

`network`
----------

This is the module dealing with the network messages. Here you define your message strucures and implement sending and
receiving them.

`user`
------

Here you implement the double-linked list, containing a node for every connected user.
As this data is shared by multiple threads, remember to use proper locking here.

`util`
------

Several utility functions, hopefully making your lives a little bit easier:

* `utilInit()`: Initialize the util module, setting the program name given.
   Must be called before any of the output functions are used.
* `normalPrint()`, `debugPrint()`, `infoPrint()` `errorPrint()`: These are `printf()`-like output functions to
  pretty-print regular, debug, informational or error messages.
  They use colors (unless disabled via `styleDisable()`) and also print the program name in front.
  Debug messages are only printed if enabled with `debugEnable()` before.
* `errnoPrint()`: This is `perror()` on steroids, using colors and a `printf()`-like prefix.
* `debugHexdump()`, `hexdump()`, `vhexdump()`: Use these to dump data in a nice hexadecimal form.
  Great for debugging or for a nice Matrix effect.
* `getProgName()`: Get the program name used by the output functions.
* `debugEnable()`, `debugDisable()`, `debugEnabled()`: Enable or disable `debugPrint()` and `debugHexdump()`, or
  get the status.
* `styleEnable()`, `styleDisable()`, `styleEnabled()`: Enable or disable colorful output, or get the status.
* `nameBytesValidate()`: Checks if a given buffer only contains bytes that are valid in user or server names.
* `ntoh64u()`, `hton64u()`: POSIX lacks a portable way to convert 64 bit values from network into host byte order and
  vice versa. Luckily, filling this gap is not too hard.
