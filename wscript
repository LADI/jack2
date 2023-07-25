#! /usr/bin/python3
# encoding: utf-8
#
# Copyright (C) 2015-2018 Karl Linden <karl.j.linden@gmail.com>
# Copyleft (C) 2008-2022 Nedko Arnaudov
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#

from __future__ import print_function

import os
import shutil
import sys

from waflib import Logs, Options, TaskGen
from waflib import Context
from waflib.Build import BuildContext, CleanContext, InstallContext, UninstallContext

# see also common/JackConstants.h
VERSION = '2.23.0'
APPNAME = 'LADI JACK'
JACK_API_VERSION = VERSION

# these variables are mandatory ('/' are converted automatically)
top = '.'
out = 'build'

# lib32 variant name used when building in mixed mode
lib32 = 'lib32'


def display_feature(conf, msg, build):
    if build:
        conf.msg(msg, 'yes', color='GREEN')
    else:
        conf.msg(msg, 'no', color='YELLOW')


def check_for_celt(conf):
    found = False
    for version in ['11', '8', '7', '5']:
        define = 'HAVE_CELT_API_0_' + version
        if not found:
            try:
                conf.check_cfg(
                        package='celt >= 0.%s.0' % version,
                        args='--cflags --libs')
                found = True
                conf.define(define, 1)
                continue
            except conf.errors.ConfigurationError:
                pass
        conf.define(define, 0)

    if not found:
        raise conf.errors.ConfigurationError


def options(opt):
    # options provided by the modules
    opt.load('compiler_cxx')
    opt.load('compiler_c')
    opt.load('autooptions')

    #opt.load('xcode6')

    opt.recurse('compat')

    # install directories
    opt.add_option(
        '--htmldir',
        type='string',
        default=None,
        help='HTML documentation directory [Default: <prefix>/share/jack-audio-connection-kit/reference/html/',
    )
    opt.add_option('--libdir', type='string', help='Library directory [Default: <prefix>/lib]')
    opt.add_option('--libdir32', type='string', help='32bit Library directory [Default: <prefix>/lib32]')
    opt.add_option('--pkgconfigdir', type='string', help='pkg-config file directory [Default: <libdir>/pkgconfig]')
    opt.add_option('--mandir', type='string', help='Manpage directory [Default: <prefix>/share/man/man1]')

    # options affecting binaries
    opt.add_option(
        '--platform',
        type='string',
        default=sys.platform,
        help='Target platform for cross-compiling, e.g. cygwin or win32',
    )
    opt.add_option('--mixed', action='store_true', default=False, help='Build with 32/64 bits mixed mode')
    opt.add_option('--debug', action='store_true', default=False, dest='debug', help='Build debuggable binaries')
    opt.add_option(
        '--static',
        action='store_true',
        default=False,
        dest='static',
        help='Build static binaries (Windows only)',
    )

    # options affecting general jack functionality
    opt.add_option(
        '--autostart',
        type='string',
        default='default',
        help='Autostart method. Possible values: "none", "dbus", "classic", "default" (none)',
    )
    opt.add_option('--profile', action='store_true', default=False, help='Build with engine profiling')
    opt.add_option('--clients', default=256, type='int', dest='clients', help='Maximum number of JACK clients')
    opt.add_option(
        '--ports-per-application',
        default=2048,
        type='int',
        dest='application_ports',
        help='Maximum number of ports per application',
    )

    opt.set_auto_options_define('HAVE_%s')
    opt.set_auto_options_style('yesno_and_hack')

    # options with third party dependencies
    doxygen = opt.add_auto_option(
            'doxygen',
            help='Build doxygen documentation',
            conf_dest='BUILD_DOXYGEN_DOCS',
            default=False)
    doxygen.find_program('doxygen')
    alsa = opt.add_auto_option(
            'alsa',
            help='Enable ALSA driver',
            conf_dest='BUILD_DRIVER_ALSA')
    alsa.check_cfg(
            package='alsa >= 1.0.18',
            args='--cflags --libs')
    firewire = opt.add_auto_option(
            'firewire',
            help='Enable FireWire driver (FFADO)',
            conf_dest='BUILD_DRIVER_FFADO')
    firewire.check_cfg(
            package='libffado >= 1.999.17',
            args='--cflags --libs')
    iio = opt.add_auto_option(
            'iio',
            help='Enable IIO driver',
            conf_dest='BUILD_DRIVER_IIO')
    iio.check_cfg(
            package='gtkIOStream >= 1.4.0',
            args='--cflags --libs')
    iio.check_cfg(
            package='eigen3 >= 3.1.2',
            args='--cflags --libs')
    portaudio = opt.add_auto_option(
            'portaudio',
            help='Enable Portaudio driver',
            conf_dest='BUILD_DRIVER_PORTAUDIO')
    portaudio.check(header_name='windows.h')  # only build portaudio on windows
    portaudio.check_cfg(
            package='portaudio-2.0 >= 19',
            uselib_store='PORTAUDIO',
            args='--cflags --libs')
    winmme = opt.add_auto_option(
            'winmme',
            help='Enable WinMME driver',
            conf_dest='BUILD_DRIVER_WINMME')
    winmme.check(
            header_name=['windows.h', 'mmsystem.h'],
            msg='Checking for header mmsystem.h')

    celt = opt.add_auto_option(
            'celt',
            help='Build with CELT')
    celt.add_function(check_for_celt)
    opt.add_auto_option(
            'tests',
            help='Build tests',
            conf_dest='BUILD_TESTS',
            default=False,
    )

    # Suffix _PKG to not collide with HAVE_OPUS defined by the option.
    opus = opt.add_auto_option(
            'opus',
            help='Build Opus netjack2')
    opus.check(header_name='opus/opus_custom.h')
    opus.check_cfg(
            package='opus >= 0.9.0',
            args='--cflags --libs',
            define_name='HAVE_OPUS_PKG')

    samplerate = opt.add_auto_option(
            'samplerate',
            help='Build with libsamplerate')
    samplerate.check_cfg(
            package='samplerate',
            args='--cflags --libs')
    db = opt.add_auto_option(
            'db',
            help='Use Berkeley DB (metadata)')
    db.check(header_name='db.h')
    db.check(lib='db')

    # this must be called before the configure phase
    opt.apply_auto_options_hack()


def detect_platform(conf):
    # GNU/kFreeBSD and GNU/Hurd are treated as Linux
    platforms = [
        # ('KEY, 'Human readable name', ['strings', 'to', 'check', 'for'])
        ('IS_LINUX',   'Linux',   ['gnu0', 'gnukfreebsd', 'linux', 'posix']),
        ('IS_FREEBSD', 'FreeBSD', ['freebsd']),
        ('IS_MACOSX',  'MacOS X', ['darwin']),
        ('IS_SUN',     'SunOS',   ['sunos']),
        ('IS_WINDOWS', 'Windows', ['cygwin', 'msys', 'win32'])
    ]

    for key, name, strings in platforms:
        conf.env[key] = False

    conf.start_msg('Checking platform')
    platform = Options.options.platform
    for key, name, strings in platforms:
        for s in strings:
            if platform.startswith(s):
                conf.env[key] = True
                conf.end_msg(name, color='CYAN')
                break

class WafToolchainFlags:
    """
    Waf helper class for handling set of CFLAGS
    and related. The flush() method will
    prepend so to allow supplied by (downstream/distro/builder) waf caller flags
    to override the upstream flags in wscript.
    TODO: upstream this or find alternative easy way of doing the same
    """
    def __init__(self, conf):
        """
        :param conf: Waf configuration object
        """
        self.conf = conf
        self.flags = {}
        for x in ('CPPFLAGS', 'CFLAGS', 'CXXFLAGS', 'LINKFLAGS'):
            self.flags[x] = []

    def flush(self):
        """
        Flush flags to the configuration object
        Prepend is used so to allow supplied by
        (downstream/distro/builder) waf caller flags
        to override the upstream flags in wscript.
        """
        for key, val in self.flags.items():
            self.conf.env.prepend_value(key, val)

    def add(self, key, val):
        """
        :param key: Set to add flags to. 'CPPFLAGS', 'CFLAGS', 'CXXFLAGS' or 'LINKFLAGS'
        :param val: string or list of strings
        """
        flags = self.flags[key]
        if isinstance(val, list):
	    #flags.extend(val)
            for x in val:
                if not isinstance(x, str):
                    raise Exception("value must be string or list of strings. ", type(x))
                flags.append(x)
        elif isinstance(val, str):
            flags.append(val)
        else:
            raise Exception("value must be string or list of strings")

    def add_cpp(self, value):
        """
        Add flag or list of flags to CPPFLAGS
        :param value: string or list of strings
        """
        self.add('CPPFLAGS', value)

    def add_c(self, value):
        """
        Add flag or list of flags to CFLAGS
        :param value: string or list of strings
        """
        self.add('CFLAGS', value)

    def add_cxx(self, value):
        """
        Add flag or list of flags to CXXFLAGS
        :param value: string or list of strings
        """
        self.add('CXXFLAGS', value)

    def add_candcxx(self, value):
        """
        Add flag or list of flags to CFLAGS and CXXFLAGS
        :param value: string or list of strings
        """
        self.add_c(value)
        self.add_cxx(value)

    def add_link(self, value):
        """
        Add flag or list of flags to LINKFLAGS
        :param value: string or list of strings
        """
        self.add('LINKFLAGS', value)

def configure(conf):
    conf.load('compiler_cxx')
    conf.load('compiler_c')

    detect_platform(conf)
    flags = WafToolchainFlags(conf)

    if conf.env['IS_WINDOWS']:
        conf.env.append_unique('CCDEFINES', '_POSIX')
        conf.env.append_unique('CXXDEFINES', '_POSIX')
        if Options.options.platform in ('msys', 'win32'):
            conf.env.append_value('INCLUDES', ['/mingw64/include'])
            conf.check(
                header_name='pa_asio.h',
                msg='Checking for PortAudio ASIO support',
                define_name='HAVE_ASIO',
                mandatory=False)

    flags.add_c('-Wall')
    flags.add_cxx(['-Wall', '-Wno-invalid-offsetof'])
    flags.add_cxx('-std=gnu++11')

    if conf.env['IS_FREEBSD']:
        conf.check(lib='execinfo', uselib='EXECINFO', define_name='EXECINFO')
        conf.check_cfg(package='libsysinfo', args='--cflags --libs')

    if not conf.env['IS_MACOSX']:
        conf.env.append_unique('LDFLAGS', '-Wl,--no-undefined')
    else:
        conf.check(lib='aften', uselib='AFTEN', define_name='AFTEN')
        conf.check_cxx(
            fragment=''
            + '#include <aften/aften.h>\n'
            + 'int\n'
            + 'main(void)\n'
            + '{\n'
            + 'AftenContext fAftenContext;\n'
            + 'aften_set_defaults(&fAftenContext);\n'
            + 'unsigned char *fb;\n'
            + 'float *buf=new float[10];\n'
            + 'int res = aften_encode_frame(&fAftenContext, fb, buf, 1);\n'
            + '}\n',
            lib='aften',
            msg='Checking for aften_encode_frame()',
            define_name='HAVE_AFTEN_NEW_API',
            mandatory=False)

        # TODO
        flags.add_cxx('-Wno-deprecated-register')

    conf.load('autooptions')

    conf.recurse('compat')

    # Check for functions.
    conf.check(
            fragment=''
            + '#define _GNU_SOURCE\n'
            + '#include <poll.h>\n'
            + '#include <signal.h>\n'
            + '#include <stddef.h>\n'
            + 'int\n'
            + 'main(void)\n'
            + '{\n'
            + '   ppoll(NULL, 0, NULL, NULL);\n'
            + '}\n',
            msg='Checking for ppoll',
            define_name='HAVE_PPOLL',
            mandatory=False)

    conf.recurse('common')

    conf.env['LIB_PTHREAD'] = ['pthread']
    conf.env['LIB_DL'] = ['dl']
    conf.env['LIB_RT'] = ['rt']
    conf.env['LIB_M'] = ['m']
    conf.env['LIB_STDC++'] = ['stdc++']
    conf.env['JACK_API_VERSION'] = JACK_API_VERSION
    conf.env['JACK_VERSION'] = VERSION

    conf.env['BUILD_WITH_PROFILE'] = Options.options.profile
    conf.env['BUILD_WITH_32_64'] = Options.options.mixed
    conf.env['BUILD_DEBUG'] = Options.options.debug
    conf.env['BUILD_STATIC'] = Options.options.static

    conf.env['BINDIR'] = conf.env['PREFIX'] + '/bin'

    if Options.options.htmldir:
        conf.env['HTMLDIR'] = Options.options.htmldir
    else:
        # set to None here so that the doxygen code can find out the highest
        # directory to remove upon install
        conf.env['HTMLDIR'] = None

    if Options.options.libdir:
        conf.env['LIBDIR'] = Options.options.libdir
    else:
        conf.env['LIBDIR'] = conf.env['PREFIX'] + '/lib'

    if Options.options.pkgconfigdir:
        conf.env['PKGCONFDIR'] = Options.options.pkgconfigdir
    else:
        conf.env['PKGCONFDIR'] = conf.env['LIBDIR'] + '/pkgconfig'

    if Options.options.mandir:
        conf.env['MANDIR'] = Options.options.mandir
    else:
        conf.env['MANDIR'] = conf.env['PREFIX'] + '/share/man/man1'

    if conf.env['BUILD_DEBUG']:
        flags.add_candcxx('-g')
        flags.add_link('-g')

    if Options.options.autostart not in ['default', 'classic', 'dbus', 'none']:
        conf.fatal('Invalid autostart value "' + Options.options.autostart + '"')

    if Options.options.autostart == 'default':
        conf.env['AUTOSTART_METHOD'] = 'none'
    else:
        conf.env['AUTOSTART_METHOD'] = Options.options.autostart

    if conf.env['AUTOSTART_METHOD'] == 'dbus':
        conf.define('USE_LIBDBUS_AUTOLAUNCH', 1)
    elif conf.env['AUTOSTART_METHOD'] == 'classic':
        conf.define('USE_CLASSIC_AUTOLAUNCH', 1)

    conf.define('CLIENT_NUM', Options.options.clients)
    conf.define('PORT_NUM_FOR_CLIENT', Options.options.application_ports)

    if conf.env['IS_WINDOWS']:
        # we define this in the environment to maintain compatibility with
        # existing install paths that use ADDON_DIR rather than have to
        # have special cases for windows each time.
        conf.env['ADDON_DIR'] = conf.env['LIBDIR'] + '/jack'
        if Options.options.platform in ('msys', 'win32'):
            conf.define('ADDON_DIR', 'jack')
            conf.define('__STDC_FORMAT_MACROS', 1)  # for PRIu64
        else:
            # don't define ADDON_DIR in config.h, use the default 'jack'
            # defined in windows/JackPlatformPlug_os.h
            pass
    else:
        conf.env['JACK_DRIVER_DIR'] = os.path.normpath(
            os.path.join(conf.env['PREFIX'],
                         'libexec',
                         'jack-driver'))
        conf.env['JACK_INTERNAL_DIR'] = os.path.normpath(
            os.path.join(conf.env['PREFIX'],
                         'libexec',
                         'jack-internal'))
        conf.define('JACK_DRIVER_DIR', conf.env['JACK_DRIVER_DIR'])
        conf.define('JACK_INTERNAL_DIR', conf.env['JACK_INTERNAL_DIR'])
        conf.define('JACK_LOCATION', os.path.normpath(os.path.join(conf.env['PREFIX'], 'bin')))

    if not conf.env['IS_WINDOWS']:
        conf.define('USE_POSIX_SHM', 1)
    conf.define('JACKMP', 1)
    if conf.env['BUILD_WITH_PROFILE']:
        conf.define('JACK_MONITOR', 1)
    conf.write_config_header('config.h', remove=False)

    if Options.options.mixed:
        conf.setenv(lib32, env=conf.env.derive())
        flags.add_c('-m32')
        flags.add_cxx('-m32')
        flags.add_cxx('-DBUILD_WITH_32_64')
        flags.add_link('-m32')
        if Options.options.libdir32:
            conf.env['LIBDIR'] = Options.options.libdir32
        else:
            conf.env['LIBDIR'] = conf.env['PREFIX'] + '/lib32'

        if conf.env['IS_WINDOWS'] and conf.env['BUILD_STATIC']:
            def replaceFor32bit(env):
                for e in env:
                    yield e.replace('x86_64', 'i686', 1)
            for env in ('AR', 'CC', 'CXX', 'LINK_CC', 'LINK_CXX'):
                conf.all_envs[lib32][env] = list(replaceFor32bit(conf.all_envs[lib32][env]))
            conf.all_envs[lib32]['LIB_REGEX'] = ['tre32']

        # libdb does not work in mixed mode
        conf.all_envs[lib32]['HAVE_DB'] = 0
        conf.all_envs[lib32]['HAVE_DB_H'] = 0
        conf.all_envs[lib32]['LIB_DB'] = []
        # no need for opus in 32bit mixed mode clients
        conf.all_envs[lib32]['LIB_OPUS'] = []
        # someone tell me where this file gets written please..
        conf.write_config_header('config.h')

    flags.flush()

    print()
    version_msg = APPNAME + "-" + VERSION
    if os.access('version.h', os.R_OK):
        data = open('version.h').read()
        m = re.match(r'^#define GIT_VERSION "([^"]*)"$', data)
        if m != None:
            version_msg += " exported from " + m.group(1)
    elif os.access('.git', os.R_OK):
        version_msg += " git revision will be checked and eventually updated during build"
    print(version_msg)

    conf.msg('Maximum JACK clients', Options.options.clients, color='NORMAL')
    conf.msg('Maximum ports per application', Options.options.application_ports, color='NORMAL')

    conf.msg('Install prefix', conf.env['PREFIX'], color='CYAN')
    conf.msg('Library directory', conf.all_envs['']['LIBDIR'], color='CYAN')
    if conf.env['BUILD_WITH_32_64']:
        conf.msg('32-bit library directory', conf.all_envs[lib32]['LIBDIR'], color='CYAN')
    conf.msg('Drivers directory', conf.env['JACK_DRIVER_DIR'], color='CYAN')
    conf.msg('Internal clients directory', conf.env['JACK_INTERNAL_DIR'], color='CYAN')
    display_feature(conf, 'Build debuggable binaries', conf.env['BUILD_DEBUG'])

    tool_flags = [
        ('C compiler flags',   ['CFLAGS', 'CPPFLAGS']),
        ('C++ compiler flags', ['CXXFLAGS', 'CPPFLAGS']),
        ('Linker flags',       ['LINKFLAGS', 'LDFLAGS'])
    ]
    for name, vars in tool_flags:
        flags = []
        for var in vars:
            flags += conf.all_envs[''][var]
        conf.msg(name, repr(flags), color='NORMAL')

    if conf.env['BUILD_WITH_32_64']:
        conf.msg('32-bit C compiler flags', repr(conf.all_envs[lib32]['CFLAGS']))
        conf.msg('32-bit C++ compiler flags', repr(conf.all_envs[lib32]['CXXFLAGS']))
        conf.msg('32-bit linker flags', repr(conf.all_envs[lib32]['LINKFLAGS']))
    display_feature(conf, 'Build with engine profiling', conf.env['BUILD_WITH_PROFILE'])
    display_feature(conf, 'Build with 32/64 bits mixed mode', conf.env['BUILD_WITH_32_64'])

    conf.msg('Autostart method', conf.env['AUTOSTART_METHOD'])

    conf.summarize_auto_options()

    print()


def init(ctx):
    for y in (BuildContext, CleanContext, InstallContext, UninstallContext):
        name = y.__name__.replace('Context', '').lower()

        class tmp(y):
            cmd = name + '_' + lib32
            variant = lib32


def obj_add_includes(bld, obj):
    if bld.env['IS_LINUX']:
        obj.includes += ['linux', 'posix']

    if bld.env['IS_FREEBSD']:
        obj.includes += ['freebsd', 'posix']

    if bld.env['IS_MACOSX']:
        obj.includes += ['macosx', 'posix']

    if bld.env['IS_SUN']:
        obj.includes += ['posix', 'solaris']

    if bld.env['IS_WINDOWS']:
        obj.includes += ['windows']


# FIXME: Is SERVER_SIDE needed?
def create_driver_obj(bld, **kw):
    if 'use' in kw:
        kw['use'] += ['serverlib']
    else:
        kw['use'] = ['serverlib']

    driver = bld(
        features=['c', 'cxx', 'cshlib', 'cxxshlib'],
        defines=['HAVE_CONFIG_H', 'SERVER_SIDE'],
        includes=['.', 'common', 'common/jack'],
        install_path='${JACK_DRIVER_DIR}/',
        **kw)

    if bld.env['IS_WINDOWS']:
        driver.env['cxxshlib_PATTERN'] = 'jack_%s.dll'
    else:
        driver.env['cxxshlib_PATTERN'] = '%s.so'

    obj_add_includes(bld, driver)

    return driver


def build_drivers(bld):
    # Non-hardware driver sources. Lexically sorted.
    dummy_src = [
        'common/JackDummyDriver.cpp'
    ]

    loopback_src = [
        'common/JackLoopbackDriver.cpp'
    ]

    net_src = [
        'common/JackNetDriver.cpp'
    ]

    netone_src = [
        'common/JackNetOneDriver.cpp',
        'common/netjack.c',
        'common/netjack_packet.c'
    ]

    proxy_src = [
        'common/JackProxyDriver.cpp'
    ]

    # Hardware driver sources. Lexically sorted.
    alsa_src = [
        'common/memops.c',
        'linux/alsa/JackAlsaDriver.cpp',
        'linux/alsa/alsa_rawmidi.c',
        'linux/alsa/alsa_seqmidi.c',
        'linux/alsa/alsa_midi_jackmp.cpp',
        'linux/alsa/generic_hw.c',
        'linux/alsa/hdsp.c',
        'linux/alsa/alsa_driver.c',
        'linux/alsa/hammerfall.c',
        'linux/alsa/ice1712.c'
    ]

    alsarawmidi_src = [
        'linux/alsarawmidi/JackALSARawMidiDriver.cpp',
        'linux/alsarawmidi/JackALSARawMidiInputPort.cpp',
        'linux/alsarawmidi/JackALSARawMidiOutputPort.cpp',
        'linux/alsarawmidi/JackALSARawMidiPort.cpp',
        'linux/alsarawmidi/JackALSARawMidiReceiveQueue.cpp',
        'linux/alsarawmidi/JackALSARawMidiSendQueue.cpp',
        'linux/alsarawmidi/JackALSARawMidiUtil.cpp'
    ]

    boomer_src = [
        'common/memops.c',
        'solaris/oss/JackBoomerDriver.cpp'
    ]

    coreaudio_src = [
        'macosx/coreaudio/JackCoreAudioDriver.mm',
        'common/JackAC3Encoder.cpp'
    ]

    coremidi_src = [
        'macosx/coremidi/JackCoreMidiInputPort.mm',
        'macosx/coremidi/JackCoreMidiOutputPort.mm',
        'macosx/coremidi/JackCoreMidiPhysicalInputPort.mm',
        'macosx/coremidi/JackCoreMidiPhysicalOutputPort.mm',
        'macosx/coremidi/JackCoreMidiVirtualInputPort.mm',
        'macosx/coremidi/JackCoreMidiVirtualOutputPort.mm',
        'macosx/coremidi/JackCoreMidiPort.mm',
        'macosx/coremidi/JackCoreMidiUtil.mm',
        'macosx/coremidi/JackCoreMidiDriver.mm'
    ]

    ffado_src = [
        'linux/firewire/JackFFADODriver.cpp',
        'linux/firewire/JackFFADOMidiInputPort.cpp',
        'linux/firewire/JackFFADOMidiOutputPort.cpp',
        'linux/firewire/JackFFADOMidiReceiveQueue.cpp',
        'linux/firewire/JackFFADOMidiSendQueue.cpp'
    ]

    freebsd_oss_src = [
        'common/memops.c',
        'freebsd/oss/JackOSSDriver.cpp'
    ]

    iio_driver_src = [
        'linux/iio/JackIIODriver.cpp'
    ]

    oss_src = [
        'common/memops.c',
        'solaris/oss/JackOSSDriver.cpp'
    ]

    portaudio_src = [
        'windows/portaudio/JackPortAudioDevices.cpp',
        'windows/portaudio/JackPortAudioDriver.cpp',
    ]

    winmme_src = [
        'windows/winmme/JackWinMMEDriver.cpp',
        'windows/winmme/JackWinMMEInputPort.cpp',
        'windows/winmme/JackWinMMEOutputPort.cpp',
        'windows/winmme/JackWinMMEPort.cpp',
    ]

    # Create non-hardware driver objects. Lexically sorted.
    create_driver_obj(
        bld,
        target='dummy',
        source=dummy_src)

    create_driver_obj(
        bld,
        target='loopback',
        source=loopback_src)

    create_driver_obj(
        bld,
        target='net',
        source=net_src,
        use=['CELT'])

    create_driver_obj(
        bld,
        target='netone',
        source=netone_src,
        use=['SAMPLERATE', 'CELT'])

    create_driver_obj(
        bld,
        target='proxy',
        source=proxy_src)

    # Create hardware driver objects. Lexically sorted after the conditional,
    # e.g. BUILD_DRIVER_ALSA.
    if bld.env['BUILD_DRIVER_ALSA']:
        create_driver_obj(
            bld,
            target='alsa',
            source=alsa_src,
            use=['ALSA'])
        create_driver_obj(
            bld,
            target='alsarawmidi',
            source=alsarawmidi_src,
            use=['ALSA'])

    if bld.env['BUILD_DRIVER_FFADO']:
        create_driver_obj(
            bld,
            target='firewire',
            source=ffado_src,
            use=['LIBFFADO'])

    if bld.env['BUILD_DRIVER_IIO']:
        create_driver_obj(
            bld,
            target='iio',
            source=iio_driver_src,
            use=['GTKIOSTREAM', 'EIGEN3'])

    if bld.env['BUILD_DRIVER_PORTAUDIO']:
        create_driver_obj(
            bld,
            target='portaudio',
            source=portaudio_src,
            use=['PORTAUDIO'])

    if bld.env['BUILD_DRIVER_WINMME']:
        create_driver_obj(
            bld,
            target='winmme',
            source=winmme_src,
            use=['WINMME'])

    if bld.env['IS_MACOSX']:
        create_driver_obj(
            bld,
            target='coreaudio',
            source=coreaudio_src,
            use=['AFTEN'],
            framework=['AudioUnit', 'CoreAudio', 'CoreServices'])

        create_driver_obj(
            bld,
            target='coremidi',
            source=coremidi_src,
            use=['serverlib'],  # FIXME: Is this needed?
            framework=['AudioUnit', 'CoreMIDI', 'CoreServices', 'Foundation'])

    if bld.env['IS_FREEBSD']:
        create_driver_obj(
            bld,
            target='oss',
            source=freebsd_oss_src)

    if bld.env['IS_SUN']:
        create_driver_obj(
            bld,
            target='boomer',
            source=boomer_src)
        create_driver_obj(
            bld,
            target='oss',
            source=oss_src)


def git_ver(self):
    bld = self.generator.bld
    header = self.outputs[0].abspath()
    if os.access('./version.h', os.R_OK):
        header = os.path.join(os.getcwd(), out, "version.h")
        shutil.copy('./version.h', header)
        data = open(header).read()
        m = re.match(r'^#define GIT_VERSION "([^"]*)"$', data)
        if m != None:
            self.ver = m.group(1)
            Logs.pprint('BLUE', "tarball from git revision " + self.ver)
        else:
            self.ver = "tarball"
        return

    if bld.srcnode.find_node('.git'):
        self.ver = bld.cmd_and_log("LANG= git rev-parse HEAD", quiet=Context.BOTH).splitlines()[0]
        if bld.cmd_and_log("LANG= git diff-index --name-only HEAD", quiet=Context.BOTH).splitlines():
            self.ver += "-dirty"

        Logs.pprint('BLUE', "git revision " + self.ver)
    else:
        self.ver = "unknown"

    fi = open(header, 'w')
    fi.write('#define GIT_VERSION "%s"\n' % self.ver)
    fi.close()


def build(bld):
    bld(rule=git_ver, target='version.h', update_outputs=True, always=True, ext_out=['.h'])
    if not bld.variant and bld.env['BUILD_WITH_32_64']:
        Options.commands.append(bld.cmd + '_' + lib32)

    # process subfolders from here
    bld.recurse('common')

    if bld.variant:
        # only the wscript in common/ knows how to handle variants
        return

    bld.recurse('compat')

    build_drivers(bld)

    if bld.env['IS_LINUX'] or bld.env['IS_FREEBSD']:
        bld.recurse('man')
    if not bld.env['IS_WINDOWS'] and bld.env['BUILD_TESTS']:
        bld.recurse('tests')

    if bld.env['BUILD_DOXYGEN_DOCS']:
        html_build_dir = bld.path.find_or_declare('html').abspath()

        bld(
            features='subst',
            source='doxyfile.in',
            target='doxyfile',
            HTML_BUILD_DIR=html_build_dir,
            SRCDIR=bld.srcnode.abspath(),
            VERSION=VERSION
        )

        # There are two reasons for logging to doxygen.log and using it as
        # target in the build rule (rather than html_build_dir):
        # (1) reduce the noise when running the build
        # (2) waf has a regular file to check for a timestamp. If the directory
        #     is used instead waf will rebuild the doxygen target (even upon
        #     install).
        def doxygen(task):
            doxyfile = task.inputs[0].abspath()
            logfile = task.outputs[0].abspath()
            cmd = '%s %s &> %s' % (task.env['DOXYGEN'][0], doxyfile, logfile)
            return task.exec_command(cmd)

        bld(
            rule=doxygen,
            source='doxyfile',
            target='doxygen.log'
        )

        # Determine where to install HTML documentation. Since share_dir is the
        # highest directory the uninstall routine should remove, there is no
        # better candidate for share_dir, but the requested HTML directory if
        # --htmldir is given.
        if bld.env['HTMLDIR']:
            html_install_dir = bld.options.destdir + bld.env['HTMLDIR']
            share_dir = html_install_dir
        else:
            share_dir = bld.options.destdir + bld.env['PREFIX'] + '/share/jack-audio-connection-kit'
            html_install_dir = share_dir + '/reference/html/'

        if bld.cmd == 'install':
            if os.path.isdir(html_install_dir):
                Logs.pprint('CYAN', 'Removing old doxygen documentation installation...')
                shutil.rmtree(html_install_dir)
                Logs.pprint('CYAN', 'Removing old doxygen documentation installation done.')
            Logs.pprint('CYAN', 'Installing doxygen documentation...')
            shutil.copytree(html_build_dir, html_install_dir)
            Logs.pprint('CYAN', 'Installing doxygen documentation done.')
        elif bld.cmd == 'uninstall':
            Logs.pprint('CYAN', 'Uninstalling doxygen documentation...')
            if os.path.isdir(share_dir):
                shutil.rmtree(share_dir)
            Logs.pprint('CYAN', 'Uninstalling doxygen documentation done.')
        elif bld.cmd == 'clean':
            if os.access(html_build_dir, os.R_OK):
                Logs.pprint('CYAN', 'Removing doxygen generated documentation...')
                shutil.rmtree(html_build_dir)
                Logs.pprint('CYAN', 'Removing doxygen generated documentation done.')


@TaskGen.extension('.mm')
def mm_hook(self, node):
    """Alias .mm files to be compiled the same as .cpp files, gcc will do the right thing."""
    return self.create_compiled_task('cxx', node)
