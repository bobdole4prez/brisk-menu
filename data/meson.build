# We'll generate resources in this build directory, so we'll need the include path
data_include = include_directories('.')

# Sort our our resources (causes meson to look for "glib-2.0" dependency)
brisk_resources = gnome.compile_resources(
    'brisk-resources',
    'brisk.gresource.xml',
    source_dir: meson.current_source_dir(),
    c_name: 'brisk_resources',
    extra_args: [
        '--manual-register',
    ],
)

# libsaver_glue provides dbus code for the MATE screensaver
libsaver_glue = gnome.gdbus_codegen(
    'libsaver-glue',
    'org.mate.ScreenSaver.xml',
    interface_prefix : 'org.mate.',
    namespace : 'Mate',
)

# libsession_glue provides dbus code for the MATE/GNOME session
libsession_glue = gnome.gdbus_codegen(
    'libsession-glue',
    'org.gnome.SessionManager.xml',
    interface_prefix : 'org.gnome.',
    namespace : 'Gnome',
)

icons = [
    'brisk_system-log-out-symbolic.svg',
]

# Install our icons
iconsdir = join_paths(path_datadir, 'icons', 'hicolor', 'scalable', 'actions')
install_data(
    icons,
    install_dir: iconsdir,
)

# Prepare to configure our files.
data_conf = configuration_data()
data_conf.set('LIBEXECDIR', path_libexecdir)
if with_in_process
    data_conf.set('IN_PROCESS', 'true')
    data_conf.set('LOCATION', join_paths(path_appletlibdir, 'libbrisk-menu-applet.so'))
else
    data_conf.set('IN_PROCESS', 'false')
    data_conf.set('LOCATION', join_paths(path_libexecdir, 'brisk-menu'))
endif
if with_wayland
    data_conf.set('PLATFORMS', 'X11;Wayland;')
else
    data_conf.set('PLATFORMS', 'X11;')
endif

# Write applet configuration
appletdir = join_paths(path_datadir, 'mate-panel', 'applets')
configure_file(
    input: 'com.solus_project.brisk.BriskMenu.mate-panel-applet.in',
    output: 'com.solus_project.brisk.BriskMenu.mate-panel-applet',
    configuration: data_conf,
    install_dir: appletdir,
)

# Write d-bus activation unit for standalone Brisk Menu
if not with_in_process
    servicedir = join_paths(path_datadir, 'dbus-1', 'services')
    configure_file(
        input: 'org.mate.panel.applet.BriskMenuFactory.service.in',
        output: 'org.mate.panel.applet.BriskMenuFactory.service',
        configuration: data_conf,
        install_dir: servicedir,
    )
endif

# Helper to toggle the menu over D-Bus; bind it to a key in Wayland compositors
install_data(
    'brisk-menu-toggle',
    install_dir: path_bindir,
    install_mode: 'rwxr-xr-x',
)

# Install gschemas
gschemadir = join_paths(path_datadir, 'glib-2.0', 'schemas')
install_data(
    'com.solus-project.brisk-menu.gschema.xml',
    install_dir: gschemadir,
)
