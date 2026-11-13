mate_applet_sources = [
    'applet.c',
    'main.c',
]

mate_applet_dependencies = [
    link_libbackend,
    link_libfrontend,
    link_libresources,
    dep_mate_applet,
    dep_notify,
]

if with_in_process
    # Loaded by mate-panel itself (libbrisk-menu-applet.so)
    mate_applet = shared_module(
        'brisk-menu-applet',
        sources: mate_applet_sources,
        dependencies: mate_applet_dependencies,
        include_directories: [
            config_h_dir,
        ],
        install_dir: path_appletlibdir,
        install: true,
    )
else
    mate_applet = executable(
        'brisk-menu',
        sources: mate_applet_sources,
        dependencies: mate_applet_dependencies,
        include_directories: [
            config_h_dir,
        ],
        install_dir: path_libexecdir,
        install: true,
    )
endif
