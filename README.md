btnx
====

btnx (Button Extension) is a daemon that enables rerouting of mouse button events through uinput as keyboard and other mouse button combinations. btnx requires btnx-config, a configuration tool for btnx. See https://github.com/cdobrich/btnx-config for more details.

History
----
btnx and btnx-config was originally written by [Olli Salonen](https://launchpad.net/~daou). However he has ceased development of the code and one of the repositories is lost. Afraid the code might be totally lost if it is not hosted somewhere, I acquired his blessing to move the project here and maintain it. (Meaning this isn't a fork.) It is now being maintained (albeit limitedly at the moment), particularly for major bugs.

Licences
----
GNU GPL v2

Programming Languages
----
C

Description
----
btnx is a daemon that sniffs events from the mouse event handler.

If it recognizes a configured event, it sends a keyboard and/or mouse combination event to uinput. This means you can configure certain mouse buttons to send keyboard and mouse events to your system with or without X.

It is useful for mice with more buttons than window managers can handle. It also means you won't need to manually edit your window manager and X configurations to get additional functionality from extra buttons.

Building
=======

## Depenencies

To build on **Ubuntu** or **Debian**, you need the following dependencies:

* `build-essential`
* `libdaemon-dev`
* `libglade2-dev`
* `libgtk2.0-dev`

On most Ubuntu machines, these are available without needing additional sources being enabled on your apt-package respositories. To install these dependencies, run the following commands on the commandline:

`sudo apt-get install libdaemon-dev libglade2-dev libgtk2.0-dev`

To build on **Fedora** or **Redhat**, you need the following dependencies:

* `libdaemon-devel`
* `libglade2-devel`

On Fedora, you may need more if you don't have GNU-Make or the C/C++ compilers (gcc/g++) installed.

To install these dependencies, run the following commands on the commandline:

`sudo dnf install libdaemon-devel libglade2-devel `

## Compiling

btnx and btnx-config follow the simple GNU-Make conventions for easy compiling, after you have the dependencies installed. Run these commands from commandline:

`./configure
make`

## Installing

btnx and btnx-config follow the simple GNU-Make conventions for installing, after you have successfully run the compiling step. Run this commands from commandline:

`sudo make install`

## Uninstalling

btnx and btnx-config follow the simple GNU-Make conventions for uninstalling:

`sudo make install`

Configuration & Service Control
======
The configuration files for btnx are located at /etc/btnx. The configuration file itself is edited using the graphical program btnx-config. You must install and run btnx-config before btnx will work.

After adjusting the configuration to your liking through btnx-config, make sure to restart the btnx service, either through the "Restart btnx" button in the btnx-config GUI or by running this command on commandline:

`$ sudo /etc/init.d/btnx restart`
