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

VERSION = "2.22.1"
APPNAME = 'jackdbus'

# these variables are mandatory ('/' are converted automatically)
top = '.'
out = 'build'

def display_feature(conf, msg, build):
    if build:
        conf.msg(msg, 'yes', color='GREEN')
    else:
        conf.msg(msg, 'no', color='YELLOW')


def options(opt):
    # options provided by the modules
    opt.load('compiler_cxx')
    opt.load('compiler_c')
    opt.load('autooptions')

    # install directories
    opt.add_option(
        '--htmldir',
        type='string',
        default=None,
        help='HTML documentation directory [Default: <prefix>/share/jack-audio-connection-kit/reference/html/',
    )
    opt.add_option('--libdir', type='string', help='Library directory [Default: <prefix>/lib]')
    opt.add_option('--pkgconfigdir', type='string', help='pkg-config file directory [Default: <libdir>/pkgconfig]')
    opt.add_option('--mandir', type='string', help='Manpage directory [Default: <prefix>/share/man/man1]')

    # options affecting binaries
    opt.add_option(
        '--platform',
        type='string',
        default=sys.platform,
        help='Target platform for cross-compiling, e.g. cygwin or win32',
    )
    opt.add_option('--debug', action='store_true', default=False, dest='debug', help='Build debuggable binaries')

    #opt.set_auto_options_define('HAVE_%s')
    #opt.set_auto_options_style('yesno_and_hack')

    # options with third party dependencies
    #doxygen = opt.add_auto_option(
    #        'doxygen',
    #        help='Build doxygen documentation',
    #        conf_dest='BUILD_DOXYGEN_DOCS',
    #        default=False)
    #doxygen.find_program('doxygen')

    # dbus options
    opt.recurse('dbus')

    # this must be called before the configure phase
    #opt.apply_auto_options_hack()


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

    conf.check_cfg(package='jackserver', uselib_store='JACKSERVER', args=["--cflags", "--libs"])

    conf.check_cfg(package='expat', args='--cflags --libs')

    conf.env.append_unique('CFLAGS', '-Wall')
    conf.env.append_unique('CXXFLAGS', ['-Wall', '-Wno-invalid-offsetof'])
    conf.env.append_unique('CXXFLAGS', '-std=gnu++11')

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
        conf.env.append_unique('CXXFLAGS', '-Wno-deprecated-register')

    conf.load('autooptions')

    conf.env['LIB_PTHREAD'] = ['pthread']
    conf.env['LIB_DL'] = ['dl']
    conf.env['LIB_RT'] = ['rt']
    conf.env['LIB_M'] = ['m']
    conf.env['LIB_STDC++'] = ['stdc++']
    conf.env['JACK_VERSION'] = VERSION

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
        conf.env.append_unique('CXXFLAGS', '-g')
        conf.env.append_unique('CFLAGS', '-g')
        conf.env.append_unique('LINKFLAGS', '-g')

    conf.define('JACK_VERSION', conf.env['JACK_VERSION'])
    conf.write_config_header('config.h', remove=False)

    conf.recurse('dbus')

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

    conf.msg('Install prefix', conf.env['PREFIX'], color='CYAN')
    conf.msg('Library directory', conf.all_envs['']['LIBDIR'], color='CYAN')
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

    #conf.summarize_auto_options()

    conf.msg('D-Bus service install directory', conf.env['DBUS_SERVICES_DIR'], color='CYAN')

    if conf.env['DBUS_SERVICES_DIR'] != conf.env['DBUS_SERVICES_DIR_REAL']:
        print()
        print(Logs.colors.RED + 'WARNING: D-Bus session services directory as reported by pkg-config is')
        print(Logs.colors.RED + 'WARNING:', end=' ')
        print(Logs.colors.CYAN + conf.env['DBUS_SERVICES_DIR_REAL'])
        print(Logs.colors.RED + 'WARNING: but service file will be installed in')
        print(Logs.colors.RED + 'WARNING:', end=' ')
        print(Logs.colors.CYAN + conf.env['DBUS_SERVICES_DIR'])
        print(
            Logs.colors.RED + 'WARNING: You may need to adjust your D-Bus configuration after installing jackdbus'
            )
        print('WARNING: You can override dbus service install directory')
        print('WARNING: with --enable-pkg-config-dbus-service-dir option to this script')
        print(Logs.colors.NORMAL, end=' ')
    print()

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

    # process subfolders from here

    if bld.env['IS_LINUX'] or bld.env['IS_FREEBSD']:
        bld.recurse('man')
    bld.recurse('dbus')

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
