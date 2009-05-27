#! /usr/bin/env python
# encoding: utf-8

import os
import Utils
import Options
import commands
g_maxlen = 40
import shutil
import Task
import re
import Logs

VERSION='0.2'
APPNAME='jacknone'
JACK_API_VERSION = '0.1.0'

# these variables are mandatory ('/' are converted automatically)
srcdir = '.'
blddir = 'build'

def display_msg(msg, status = None, color = None):
    sr = msg
    global g_maxlen
    g_maxlen = max(g_maxlen, len(msg))
    if status:
        print "%s :" % msg.ljust(g_maxlen),
        Utils.pprint(color, status)
    else:
        print "%s" % msg.ljust(g_maxlen)

def display_feature(msg, build):
    if build:
        display_msg(msg, "yes", 'GREEN')
    else:
        display_msg(msg, "no", 'YELLOW')

def create_svnversion_task(bld, header='svnversion.h', define=None):
    import Constants, Build

    cmd = '../svnversion_regenerate.sh ${TGT}'
    if define:
        cmd += " " + define

    cls = Task.simple_task_type('svnversion', cmd, color='BLUE', before='cc')
    cls.runnable_status = lambda self: Constants.RUN_ME

    def post_run(self):
        sg = Utils.h_file(self.outputs[0].abspath(self.env))
        #print sg.encode('hex')
        Build.bld.node_sigs[self.env.variant()][self.outputs[0].id] = sg
    cls.post_run = post_run

    tsk = cls(bld.env.copy())
    tsk.inputs = []
    tsk.outputs = [bld.path.find_or_declare(header)]

def set_options(opt):
    # options provided by the modules
    opt.tool_options('compiler_cxx')
    opt.tool_options('compiler_cc')

    opt.add_option('--libdir', type='string', help="Library directory [Default: <prefix>/lib]")
    opt.add_option('--classic', action='store_true', default=False, help='Enable standard JACK (jackd)')
    opt.add_option('--dbus', action='store_true', default=False, help='Enable D-Bus JACK (jackdbus)')
    opt.add_option('--doxygen', action='store_true', default=False, help='Enable build of doxygen documentation')
    opt.add_option('--profile', action='store_true', default=False, help='Build with engine profiling')
    opt.add_option('--mixed', action='store_true', default=False, help='Build with 32/64 bits mixed mode')
    opt.add_option('--clients', default=64, type="int", dest="clients", help='Maximum number of JACK clients')
    opt.add_option('--ports', default=1024, type="int", dest="ports", help='Maximum number of ports')
    opt.add_option('--clients', default=64, type="int", dest="clients", help='Maximum number of JACK clients')
    opt.add_option('--ports', default=1024, type="int", dest="ports", help='Maximum number of ports')
    opt.sub_options('dbus')

def configure(conf):
    platform = Utils.detect_platform()
    conf.env['IS_MACOSX'] = platform == 'darwin'
    conf.env['IS_LINUX'] = platform == 'linux'
    conf.env['IS_SUN'] = platform == 'sunos'

    if conf.env['IS_LINUX']:
        Utils.pprint('CYAN', "Linux detected")

    if conf.env['IS_MACOSX']:
        Utils.pprint('CYAN', "MacOS X detected")

    if conf.env['IS_SUN']:
        Utils.pprint('CYAN', "SunOS detected")

    if conf.env['IS_LINUX']:
        conf.check_tool('compiler_cxx')
        conf.check_tool('compiler_cc')

    if conf.env['IS_MACOSX']:
        conf.check_tool('compiler_cxx')
        conf.check_tool('compiler_cc')

    # waf 1.5 : check_tool('compiler_cxx') and check_tool('compiler_cc') do not work correctly, so explicit use of gcc and g++
    if conf.env['IS_SUN']:
        conf.check_tool('g++')
        conf.check_tool('gcc')

    #if conf.env['IS_SUN']:
    #   conf.check_tool('compiler_cxx')
    #   conf.check_tool('compiler_cc')
 
    conf.env.append_unique('CXXFLAGS', '-O3 -Wall')
    conf.env.append_unique('CCFLAGS', '-O3 -Wall')

    conf.sub_config('common')
    if conf.env['IS_LINUX']:
        conf.sub_config('linux')
    if Options.options.dbus:
        conf.sub_config('dbus')
    conf.sub_config('example-clients')

    conf.env['LIB_PTHREAD'] = ['pthread']
    conf.env['LIB_DL'] = ['dl']
    conf.env['LIB_RT'] = ['rt']
    conf.env['JACK_API_VERSION'] = JACK_API_VERSION
    conf.env['JACK_VERSION'] = VERSION

    conf.env['BUILD_DOXYGEN_DOCS'] = Options.options.doxygen
    conf.env['BUILD_WITH_PROFILE'] = Options.options.profile
    conf.env['BUILD_WITH_32_64'] = Options.options.mixed
    conf.env['BUILD_JACKDBUS'] = Options.options.dbus
    conf.env['BUILD_JACKD'] = Options.options.classic

    if Options.options.libdir:
        conf.env['LIBDIR'] = Options.options.libdir
    else:
        conf.env['LIBDIR'] = conf.env['PREFIX'] + '/lib/'

    conf.define('CLIENT_NUM', Options.options.clients)
    conf.define('PORT_NUM', Options.options. ports)

    conf.define('ADDON_DIR', os.path.normpath(os.path.join(conf.env['LIBDIR'], 'jack')))
    conf.define('JACK_LOCATION', os.path.normpath(os.path.join(conf.env['PREFIX'], 'bin')))
    conf.define('USE_POSIX_SHM', 1)
    conf.define('JACKMP', 1)
    if conf.env['BUILD_JACKDBUS'] == True:
        conf.define('JACK_DBUS', 1)
    if conf.env['BUILD_WITH_PROFILE'] == True:
        conf.define('JACK_MONITOR', 1)
    if conf.env['BUILD_WITH_32_64'] == True:
        conf.define('JACK_32_64', 1)
    conf.write_config_header('config.h')

    svnrev = None
    if os.access('svnversion.h', os.R_OK):
        data = file('svnversion.h').read()
        m = re.match(r'^#define SVN_VERSION "([^"]*)"$', data)
        if m != None:
            svnrev = m.group(1)

    print
    display_msg("==================")
    version_msg = "JACK NONE" + VERSION
    if svnrev:
        version_msg += " exported from r" + svnrev
    else:
        version_msg += " svn revision will checked and eventually updated during build"
    print version_msg

    print "Build with a maximum of %d JACK clients" % conf.env['CLIENT_NUM']
    print "Build with a maximum of %d ports" % conf.env['PORT_NUM']
 
    display_msg("Install prefix", conf.env['PREFIX'], 'CYAN')
    display_msg("Library directory", conf.env['LIBDIR'], 'CYAN')
    display_msg("Drivers directory", conf.env['ADDON_DIR'], 'CYAN')
    display_feature('Build doxygen documentation', conf.env['BUILD_DOXYGEN_DOCS'])
    display_feature('Build with engine profiling', conf.env['BUILD_WITH_PROFILE'])
    display_feature('Build with 32/64 bits mixed mode', conf.env['BUILD_WITH_32_64'])
    if conf.env['BUILD_JACKDBUS'] and conf.env['BUILD_JACKD']:
        display_feature('Build standard (jackd) and D-Bus JACK (jackdbus) : WARNING !! mixing both program may cause issues...', True)
    elif conf.env['BUILD_JACKDBUS']:
        display_feature('Build D-Bus JACK (jackdbus)', True)
    else:
        conf.env['BUILD_JACKD'] = True;  # jackd is always built be default
        display_feature('Build standard JACK (jackd)', True)
    
    if conf.env['IS_LINUX']:
        display_feature('Build with ALSA support', conf.env['BUILD_DRIVER_ALSA'] == True)
        display_feature('Build with FireWire (FreeBob) support', conf.env['BUILD_DRIVER_FREEBOB'] == True)
        display_feature('Build with FireWire (FFADO) support', conf.env['BUILD_DRIVER_FFADO'] == True)
       
    if conf.env['BUILD_JACKDBUS'] == True:
        display_msg('D-Bus service install directory', conf.env['DBUS_SERVICES_DIR'], 'CYAN')
        #display_msg('Settings persistence', xxx)

        if conf.env['DBUS_SERVICES_DIR'] != conf.env['DBUS_SERVICES_DIR_REAL']:
            print
            print Logs.colors.RED + "WARNING: D-Bus session services directory as reported by pkg-config is"
            print Logs.colors.RED + "WARNING:",
            print Logs.colors.CYAN + conf.env['DBUS_SERVICES_DIR_REAL']
            print Logs.colors.RED + 'WARNING: but service file will be installed in'
            print Logs.colors.RED + "WARNING:",
            print Logs.colors.CYAN + conf.env['DBUS_SERVICES_DIR']
            print Logs.colors.RED + 'WARNING: You may need to adjust your D-Bus configuration after installing jackdbus'
            print 'WARNING: You can override dbus service install directory'
            print 'WARNING: with --enable-pkg-config-dbus-service-dir option to this script'
            print Logs.colors.NORMAL,
    print

def build(bld):
    print ("make[1]: Entering directory `" + os.getcwd() + "/" + blddir + "'" )
    if not os.access('svnversion.h', os.R_OK):
        create_svnversion_task(bld)

   # process subfolders from here
    bld.add_subdirs('common')
    if bld.env['IS_LINUX']:
        bld.add_subdirs('linux')
        bld.add_subdirs('example-clients')
        bld.add_subdirs('tests')
        if bld.env['BUILD_JACKDBUS'] == True:
           bld.add_subdirs('dbus')
  
    if bld.env['IS_MACOSX']:
        bld.add_subdirs('macosx')
        bld.add_subdirs('example-clients')
        bld.add_subdirs('tests')
        if bld.env['BUILD_JACKDBUS'] == True:
            bld.add_subdirs('dbus')

    if bld.env['IS_SUN']:
        bld.add_subdirs('solaris')
        bld.add_subdirs('example-clients')
        bld.add_subdirs('tests')
        if bld.env['BUILD_JACKDBUS'] == True:
            bld.add_subdirs('dbus')

    if bld.env['BUILD_DOXYGEN_DOCS'] == True:
        share_dir = bld.env.get_destdir() + bld.env['PREFIX'] + '/share/jack-audio-connection-kit'
        html_docs_source_dir = "build/default/html"
        html_docs_install_dir = share_dir + '/reference/html/'
        if Options.commands['install']:
            if os.path.isdir(html_docs_install_dir):
                Utils.pprint('CYAN', "Removing old doxygen documentation installation...")
                shutil.rmtree(html_docs_install_dir)
                Utils.pprint('CYAN', "Removing old doxygen documentation installation done.")
            Utils.pprint('CYAN', "Installing doxygen documentation...")
            shutil.copytree(html_docs_source_dir, html_docs_install_dir)
            Utils.pprint('CYAN', "Installing doxygen documentation done.")
        elif Options.commands['uninstall']:
            Utils.pprint('CYAN', "Uninstalling doxygen documentation...")
            if os.path.isdir(share_dir):
                shutil.rmtree(share_dir)
            Utils.pprint('CYAN', "Uninstalling doxygen documentation done.")
        elif Options.commands['clean']:
            if os.access(html_docs_source_dir, os.R_OK):
                Utils.pprint('CYAN', "Removing doxygen generated documentation...")
                shutil.rmtree(html_docs_source_dir)
                Utils.pprint('CYAN', "Removing doxygen generated documentation done.")
        elif Options.commands['build']:
            if not os.access(html_docs_source_dir, os.R_OK):
                os.popen("doxygen").read()
            else:
                Utils.pprint('CYAN', "doxygen documentation already built.")

def dist_hook():
    os.remove('svnversion_regenerate.sh')
    os.system('../svnversion_regenerate.sh svnversion.h')
