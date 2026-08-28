Verne
=====

Verne is a file manager for the desktop, presented with GTK4 and libadwaita.

It is a fork of [Nemo](https://github.com/linuxmint/nemo), the official file
manager of the Cinnamon desktop environment, developed by
[Linux Mint](https://linuxmint.com). Nemo is in turn a fork of
[GNOME Files](https://gitlab.gnome.org/GNOME/nautilus) (formerly Nautilus) 3.4.

Almost everything Verne does — the file model, directory monitoring,
copy/move/trash operations, search, thumbnails, bookmarks, the extension API,
the actions system, the settings schemas — is the work of the Nemo and Nautilus
developers. This tree changes the presentation layer: the UI is ported from
GTK3 to GTK4 + libadwaita, and the application is branded **Verne**.

Credit and licensing
--------------------

Verne is released under the GNU General Public License version 2 or later, the
same terms as Nemo and Nautilus. See `COPYING`, `COPYING.LIB` and
`COPYING.EXTENSIONS`.

| Project | Upstream |
| --- | --- |
| Nemo (direct parent) | <https://github.com/linuxmint/nemo> — © Linux Mint and contributors |
| GNOME Files / Nautilus (original) | <https://gitlab.gnome.org/GNOME/nautilus> — © the GNOME Project, Eazel, Inc. and Red Hat, Inc. |
| Icons and visual design | [GNOME Design Team](https://gitlab.gnome.org/Teams/Design) |

Please report Verne-specific problems here, and problems that reproduce in Nemo
itself to the Nemo issue tracker.

Compatibility with Nemo
-----------------------

Verne deliberately keeps Nemo's internal APIs, D-Bus names, GSettings schemas
(`org.nemo.*`), extension directory and action format. Nemo actions and Nemo
extensions built against the GTK4 extension API work unchanged.

History
-------

Nemo started as a fork of the GNOME file manager Nautilus v3.4. Version 1.0.0
was released in July 2012 along with version 1.6 of Cinnamon, reaching version
1.1.2 in November 2012.

Developer Gwendal Le Bihan named the project "nemo" after Jules Verne's famous
character Captain Nemo, who is the captain of the Nautilus. This fork continues
that naming line: the Nautilus carried Nemo, and both were written by Verne.

Features
--------

1. Ability to SSH into remote servers
2. Native support for FTP (File Transfer Protocol) and MTP (Media Transfer Protocol)
3. All the features Nautilus 3.4 had and which are missing in Nautilus 3.6 (all desktop icons, compact view, etc.)
4. Open in terminal (integral part of the file manager)
5. Open as root (integral part of the file manager)
6. Uses GVfs and GIO
7. File operations progress information (when copying or moving files, one can see the percentage and information about the operation on the window title and so also in the window list)
8. Proper GTK bookmarks management
9. Full navigation options (back, forward, up, refresh)
10. Ability to toggle between the path entry and the path breadcrumb widgets
11. Many more configuration options

Building
--------

```
meson setup build --prefix=/usr
ninja -C build
ninja -C build install
```

Build dependencies are listed in `debian/control`. GTK 4.10 or newer and
libadwaita 1.4 or newer are required.

On a Cinnamon system, add `-Dcinnamon_schemas=false`. Verne ships a cut-down
copy of the `org.cinnamon.desktop` GSettings schemas so that it can run outside
Cinnamon at all; installing that copy over the full schemas from
`cinnamon-desktop-data` would break other Cinnamon applications.
