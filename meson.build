project(
    'xdg-desktop-portal-termfilechooser',
    'c',
    version: '0.0.1',
    license: 'GPL-3.0-or-later',
    meson_version: '>=0.59.0',
    default_options: ['warning_level=3', 'werror=true'],
)

cc = meson.get_compiler('c')
add_project_arguments(cc.get_supported_arguments([
    '-Wno-unused-parameter',
    '-Wno-gnu-zero-variadic-macro-arguments',
    '-D_POSIX_C_SOURCE=200809L',
]), language: 'c')

rt_dep = cc.find_library('rt')

if get_option('sd-bus-provider') == 'auto'
    assert(get_option('auto_features').auto(),
           'sd-bus-provider must not be set to auto since auto_features != auto')
    sdbus_dep = dependency('libsystemd',
        required: false,
        not_found_message: 'libsystemd not found, trying libelogind',
    )
    if not sdbus_dep.found()
        sdbus_dep = dependency('libelogind',
            required: false,
            not_found_message: 'libelogind not found, trying basu',
        )
    endif
    if not sdbus_dep.found()
        sdbus_dep = dependency('basu',
            required: false,
        )
    endif
    if not sdbus_dep.found()
        error('Neither libsystemd, nor libelogind, nor basu was found')
    endif
else
    sdbus_dep = dependency(get_option('sd-bus-provider'))
endif
add_project_arguments('-DHAVE_' + sdbus_dep.name().to_upper() + '=1', language: 'c')


prefix = get_option('prefix') # /
sysconfdir = get_option('sysconfdir') # /etc
add_project_arguments('-DSYSCONFDIR="@0@"'.format(prefix / sysconfdir), language: 'c')

conf_data = configuration_data()
conf_data.set('libexecdir', get_option('prefix') / get_option('libexecdir')) # /usr/libexec
conf_data.set('systemd_service', '')

systemd = dependency('systemd', required: get_option('systemd'))
if systemd.found()
    systemd_service_file = 'xdg-desktop-portal-termfilechooser.service'
    user_unit_dir = systemd.get_variable(pkgconfig: 'systemduserunitdir',
    pkgconfig_define: ['prefix', get_option('prefix')])
    conf_data.set('systemd_service', 'SystemdService=' + systemd_service_file)

    configure_file(
        configuration: conf_data,
        input: 'assets/' + systemd_service_file + '.in',
        output: '@BASENAME@',
        install_dir: user_unit_dir,
    )
endif

configure_file(
    configuration: conf_data,
    input: 'assets/org.freedesktop.impl.portal.desktop.termfilechooser.service.in',
    output: '@BASENAME@',
    install_dir: get_option('datadir') / 'dbus-1' / 'services',
)

install_data(
    'assets/termfilechooser.portal',
    install_dir: get_option('datadir') / 'xdg-desktop-portal' / 'portals',
)

executable('xdg-desktop-portal-termfilechooser',
    'src/xdptf.c',
    'src/log.c',
    'src/dbus.c',
    'src/picker.c',
    'src/uri.c',
    'src/filechooser.c',
    'src/xmalloc.c',
    'src/ds.c',
    'src/config.c',
    'src/pollen_impl.c',
    include_directories: [
        'lib'
    ],
    dependencies: [
        sdbus_dep,
        rt_dep,
    ],
    install: true,
    install_dir: get_option('libexecdir'),
)

