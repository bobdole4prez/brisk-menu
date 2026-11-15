brisk-menu (Wayland fork)
=========================

> [!IMPORTANT]
> **This fork is not actively developed.** It exists to share a working Wayland build of Brisk Menu for anyone who wants it. Upstream ([getsolus/brisk-menu](https://github.com/getsolus/brisk-menu)) was archived in 2025. It's provided as-is: Issues are turned off, and there's no promise of fixes or new releases. Pull requests may not be reviewed or merged.

This fork adds Wayland support to Brisk Menu for MATE 1.27 and later (mate-panel's Wayland session, e.g. mate-wayland-session with Wayfire). It runs unchanged in an X11 session too.

Download a Debian 13 (trixie) package from the [Releases](https://github.com/bobdole4prez/brisk-menu/releases) page. **Pick the one that matches your mate-panel version** (`dpkg-query -W mate-panel`):

| Your mate-panel | Package to install |
|---|---|
| 1.27 or 1.28 (includes Debian 13's own 1.27.1) | `mate-applet-brisk-menu_…+mate1.28_amd64.deb` |
| 1.29 or later | `mate-applet-brisk-menu_…+mate1.29_amd64.deb` |

mate-panel 1.29 changed its applet library (`libmate-panel-applet-4.so.2` and a larger applet class), so each package works only with the mate-panel series it was built for; their dependencies stop apt from installing the wrong one. With the wrong build, the panel reports *"The panel encountered a problem while loading BriskMenuFactory::BriskMenu"*.

If neither package fits your system, rebuild from the source package (`.dsc`, `.orig.tar.xz`, `.debian.tar.xz`) against your installed mate-panel; the build labels itself to match:

```bash
dpkg-source -x brisk-menu_0.6.2+wayland1-2.dsc
cd brisk-menu-0.6.2+wayland1
dpkg-checkbuilddeps            # install anything listed; needs your mate-panel's libmate-panel-applet-dev
dpkg-buildpackage -us -uc -b
sudo apt install ../mate-applet-brisk-menu_*_amd64.deb
mate-panel --replace &
```

### What's different from upstream

- The applet is built **in-process** (a module mate-panel loads) instead of as a separate program. mate-panel only supports separate-program applets on X11, which is why upstream Brisk shows nothing under Wayland.
- Under Wayland the menu is a **layer-shell** surface (gtk-layer-shell) anchored to the panel edge; clicking outside it or pressing Escape closes it.
- Wayland doesn't let applications grab global keys, so the Super key is bound in the compositor instead, to the new `brisk-menu-toggle` command. For Wayfire, in `wayfire.ini`:

  ```
  [command]
  binding_brisk = <super>
  command_brisk = brisk-menu-toggle
  ```

- Build options: `-Din-process=true|false` (default true; false restores the X11-only upstream behaviour) and `-Dwayland=auto|enabled|disabled` (needs in-process and `gtk-layer-shell-0` >= 0.6).

### Known limitations

- Under Wayland, **Log Out and Shut Down need [mate-session-manager 1.29.0](https://github.com/mate-desktop/mate-session-manager/releases/tag/v1.29.0) or later**, with the MATE Wayland session it provides chosen at login. With older versions they do nothing, because the MATE session service isn't running in the Wayland session (mate-panel's own menu has the same problem, [mate-panel#1432](https://github.com/mate-desktop/mate-panel/issues/1432)). **Lock still does nothing under Wayland**: it uses the MATE screensaver, which only works on X11.
- Menu placement assumes the panel starts at the edge of the monitor (true for normal full-length panels).
- Because the applet runs inside mate-panel, a crash in Brisk takes the panel down with it.
- Tested on Debian 13 with MATE 1.29.2 (built from the mate-desktop sources) under Wayfire and in an X11 session, and with Debian 13's own MATE packages (mate-panel 1.27.1).

### Building the Debian package

```bash
sudo apt build-dep ./      # or install the Build-Depends in debian/control
dpkg-buildpackage -us -uc -b
```

Or with meson directly, as below.

---

The upstream README follows.

brisk-menu
==========

Solus no longer maintains a MATE edition. There is no continued interest in this project, it is no longer maintained.

The original README is below.

---

[![Coverity Scan](https://img.shields.io/coverity/scan/11139.svg)](https://scan.coverity.com/projects/solus-project-brisk-menu) [![License](https://img.shields.io/badge/License-GPL%202.0-blue.svg)](https://opensource.org/licenses/GPL-2.0)

brisk-menu is a modern and efficient menu designed to improve the MATE Desktop Environment with modern, first-class options.

The purpose of this project is to provide a usable menu as seen in other desktops without the bloat and performance issues.

brisk-menu is **distro-agnostic** and the reporting of portability issues is encouraged.

![screenshot](https://raw.githubusercontent.com/getsolus/brisk-menu/master/.github/main.png)

brisk-menu is a **collaborative** project between [Solus](https://getsol.us/) and [Ubuntu MATE](https://ubuntu-mate.org/)

![ubuntu_mate_logo](https://ubuntu-mate.org/images/get-involved/logo-guidelines/Main_Logo.png) ![solus_logo](https://build.getsol.us/logo.png)

Features
--------

 - Keyboard centric (mice welcome too, of course)
 - Hotkey support (defaults to <kbd>Super</kbd>, configurable in gsettings)
 - Stupid-fast
 - Efficient, useful searching with prioritised listings
 - Modular backend design split from the frontend, allowing new backends in future
   (hint: the frontend is not tied to `.desktop` files)
 - Context menus for `.desktop` actions (incognito mode, etc.)
 - Pin shortcuts to the `Favourites` backend and directly to the desktop using the context menu (unpin too!)
 - Configurable label (hide/text)
 - Automatically adapt to vertical panels
 - Automatically reload
 - Filter via categories
 - Session/screensaver controls
 - Drag & drop support for launchers
 - Sidebar launcher support
 - GTK3 + CSS styling options
 - Fully correct `X11` WM integration (grab policy and window types)
 - Not Python.

**Planned**

These planned features will be implemented in the future:

 - Settings UI to control further visual aspects (labels/icons/options)
 - Improved styling for the window edge + search entry.

More will be added.

![screenshot_context](https://raw.githubusercontent.com/getsolus/brisk-menu/master/.github/context.png)

Building Brisk Menu
-------------------
**Requirements:**

 * GTK 3.18 or greater
 * GTK 3.18 build of MATE 1.22 or greater
 * **Modern** `meson` (`0.40.x`+) and `ninja` (Ubuntu users should use xenial-backports)

**Build Process:**

```bash
    meson --buildtype plain build --prefix=/usr
    ninja -C build -j$(($(getconf _NPROCESSORS_ONLN)+1))
    sudo ninja -C build install
````

**Development on Solus:**

```bash
    meson build --buildtype debugoptimized --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib64 --libexecdir=/usr/lib64/brisk-menu
    ninja -C build -j$(($(getconf _NPROCESSORS_ONLN)+1))
    sudo ninja -C build install
```

License
--------

Copyright © 2016-2020 Brisk Menu Developers

`brisk-menu` is available under the terms of the `GPL-2.0` license.

The `brisk_system-log-out-symbolic.svg` icon is a copy of `application-exit-symbolic.svg`
to use within Brisk.

This icon is copyright © Sam Hewitt, from the [Paper Icons](https://github.com/snwh/paper-icon-theme) theme, available
under the terms of the [CC-BY-SA-4.0 license](https://creativecommons.org/licenses/by-sa/4.0/).
