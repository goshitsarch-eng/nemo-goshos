verne_compat_inc = include_directories('.')

gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0')

verne_compat_lib = static_library('verne-compat',
  [
    'verne-gtk-vfuncs.c',
    'verne-gtk-container.c',
    'verne-gtk-action.c',
    'verne-gtk-uimanager.c',
    'verne-gtk-widgets.c',
    'verne-gtk-clipboard.c',
    'verne-gtk-dnd.c',
    'verne-gtk-extra.c',
    'verne-gtk-builder.c',
    'verne-gail.c',
    'verne-stubs.c',
    'verne-thumbnails.c',
  ],
  dependencies: [ gtk, adwaita, atk, glib, gobject, gdk_pixbuf_dep, x11, xi ],
  include_directories: [ rootInclude, verne_compat_inc ],
  c_args: [
    '-DG_LOG_DOMAIN="VerneCompat"',
    '-DVERNE_UNINSTALLED_SCHEMA_DIR="@0@"'.format(meson.project_build_root() / 'data'),
  ],
)

verne_compat = declare_dependency(
  include_directories: verne_compat_inc,
  link_with: verne_compat_lib,
  dependencies: [ gtk, adwaita, atk, gdk_pixbuf_dep ],
)
