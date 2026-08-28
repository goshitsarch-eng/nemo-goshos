
################################################################################
dataConf = configuration_data()
dataConf.set('VERSION', meson.project_version())

desktopFiles = [
  configure_file(
    input : 'nemo-autorun-software.desktop.in',
    output: 'nemo-autorun-software.desktop',
    configuration: dataConf,
  ),

  configure_file(
    input : 'nemo-autostart.desktop.in',
    output: 'nemo-autostart.desktop',
    configuration: dataConf,
  ),

  configure_file(
    input : 'nemo.desktop.in',
    output: 'nemo.desktop',
    configuration: dataConf,
  )
]

install_data(desktopFiles,
  install_dir: join_paths(get_option('datadir'), 'applications')
)

metainfoConf = configuration_data()
metainfoConf.set('VERSION', meson.project_version())

install_data(
  configure_file(
    input : 'nemo.metainfo.xml.in',
    output: 'nemo.metainfo.xml',
    configuration: metainfoConf,
  ),
  install_dir: join_paths(get_option('datadir'), 'metainfo')
)

################################################################################
dataConf = configuration_data()
dataConf.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))

dbusFiles = [
  configure_file(
    input : 'nemo.service.in',
    output: 'nemo.service',
    configuration: dataConf,
  ),

  configure_file(
    input : 'nemo.FileManager1.service.in',
    output: 'nemo.FileManager1.service',
    configuration: dataConf,
  ),
]

install_data(dbusFiles,
  install_dir: join_paths(get_option('datadir'), 'dbus-1', 'services')
)

################################################################################

mimeFile_translated = custom_target('nemo.xml',
  input : ['nemo.xml.in'],
  output: 'nemo.xml',
  command: ['intltool-merge',
    '--quiet',
    '--xml-style',
    '--utf8',
    po_subdir, '@INPUT@', '@OUTPUT@'
  ],
  install: true,
  install_dir: join_paths(get_option('datadir'), 'mime', 'packages'),
  build_by_default: true,
)

################################################################################
dataConf = configuration_data()
dataConf.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))

rootPolicyFile = configure_file(
  input : 'org.nemo.root.policy.in',
  output: 'org.nemo.root.policy',
  configuration: dataConf,
)

install_data(rootPolicyFile,
  install_dir: join_paths(get_option('datadir'), 'polkit-1', 'actions')
)

foreach lang_file: ['nemo_action.lang', 'nemo_search_helper.lang']
  foreach sv_dir: ['gtksourceview-2.0', 'gtksourceview-3.0', 'gtksourceview-4']
    install_data(lang_file,
      install_dir: join_paths(get_option('datadir'), sv_dir, 'language-specs')
    )
  endforeach
endforeach

################################################################################

subdir('icons')

################################################################################
# Compile schemas into the build tree so uninstalled Verne can launch.

glib_compile_schemas = find_program('glib-compile-schemas')

# This is a cut-down copy carrying only the org.cinnamon.desktop keys Verne
# reads, so that it runs outside Cinnamon at all (g_settings_new() aborts on a
# missing schema). On a Cinnamon system cinnamon-desktop-data ships the full
# schemas and installing this over them would break every other Cinnamon app,
# so packagers there must configure with -Dcinnamon_schemas=false.
if get_option('cinnamon_schemas')
  install_data('org.cinnamon.desktop.gschema.xml',
    install_dir: join_paths(get_option('datadir'), 'glib-2.0', 'schemas')
  )
endif

configure_file(
  input: 'org.cinnamon.desktop.gschema.xml',
  output: 'org.cinnamon.desktop.gschema.xml',
  copy: true,
)
configure_file(
  input: meson.project_source_root() / 'libnemo-private' / 'org.nemo.gschema.xml',
  output: 'org.nemo.gschema.xml',
  copy: true,
)

custom_target('gschemas.compiled',
  output: 'gschemas.compiled',
  command: [glib_compile_schemas, meson.current_build_dir()],
  depend_files: [
    'org.cinnamon.desktop.gschema.xml',
    meson.project_source_root() / 'libnemo-private' / 'org.nemo.gschema.xml',
  ],
  build_by_default: true,
)
