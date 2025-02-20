# xdg-desktop-portal-termfilechooser

An [xdg-desktop-portal] filechooser backend. It lets you write your own file
pickers without being tied to any specific GUI toolkit or window manager.

Spiritual successor of [GermainZ/xdg-desktop-portal-termfilechooser], which
itself is based on [xdg-desktop-portal-wlr]. Even though I started it as a
fork of the aforementioned project, basically all of the code was rewritten
(for better or for worse), so now it's kind of a ship of Theseus.

I decided to write this when my GTK theme suddenly broke for no reason and
I got flashbanged by light theme GTK filepicker. I now have permanent retina
damage.

## Compile
Prerequisites: sd-bus (either of libsystemd, libelogind, or basu).
```sh
meson setup build
meson compile -C build
```

## Usage
TODO: write this section

## License
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This project uses queue.h header which is licensed under the BSD-3-Clause license.

## References
- https://github.com/flatpak/xdg-desktop-portal
- https://github.com/emersion/xdg-desktop-portal-wlr
- https://github.com/GermainZ/xdg-desktop-portal-termfilechooser
- https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.FileChooser.html
- https://www.freedesktop.org/software/systemd/man/latest/sd-bus.html

[xdg-desktop-portal]: https://github.com/flatpak/xdg-desktop-portal
[GermainZ/xdg-desktop-portal-termfilechooser]: https://github.com/GermainZ/xdg-desktop-portal-termfilechooser
[xdg-desktop-portal-wlr]: https://github.com/emersion/xdg-desktop-portal-wlr
