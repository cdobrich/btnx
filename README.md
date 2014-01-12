btnx
====

btnx (Button Extension) is a daemon that enables rerouting of mouse button events through uinput as keyboard and other mouse button combinations. btnx requires btnx-config, a configuration tool for btnx. See https://github.com/btnx-config/ for more details.

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
If it recognizes a configured event, it sends a keyboard and/or mouse
combination event to uinput. This means you can configure certain
mouse buttons to send keyboard and mouse events to your system with or without X.

It is useful for mice with more buttons than window managers can
handle. It also means you won't need to manually edit your window
manager and X configurations to get additional functionality from
extra buttons.

The configuration files for btnx are located at /etc/btnx.
The configuration file is edited with btnx-config. You must install
and run btnx-config before btnx will work.

After changing the config file, make sure to restart btnx in btnx-config
or by running

$ sudo /etc/init.d/btnx restart
