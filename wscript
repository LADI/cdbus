#! /usr/bin/env python
# encoding: utf-8

from __future__ import with_statement

parallel_debug = False

APPNAME='cdbus'
VERSION='1.1'

# these variables are mandatory ('/' are converted automatically)
top = '.'
out = 'build'

import os, re, shutil

from waflib import Options, Logs
from waflib import Context

def display_msg(conf, msg="", status = None, color = None):
    if status:
        conf.msg(msg, status, color)
    else:
        Logs.pprint('NORMAL', msg)

def display_raw_text(conf, text, color = 'NORMAL'):
    Logs.pprint(color, text, sep = '')

def display_line(conf, text, color = 'NORMAL'):
    Logs.pprint(color, text, sep = os.linesep)

def yesno(bool):
    if bool:
        return "yes"
    else:
        return "no"

def options(opt):
    opt.load('compiler_c')
    opt.add_option('--debug', action='store_true', default=False, dest='debug', help="Build debuggable binaries")
#    opt.add_option('--distnodeps', action='store_true', default=False, help="When creating distribution tarball, don't package git submodules")
#    opt.add_option('--distname', type='string', default=None, help="Name for the distribution tarball")
#    opt.add_option('--distsuffix', type='string', default="", help="String to append to the distribution tarball name")
#    opt.add_option('--tagdist', action='store_true', default=False, help='Create of git tag for distname')
    opt.add_option('--libdir', type=str, default=None, help='Define lib dir')
    opt.add_option('--docdir', type=str, default=None, help="Define doc dir [default: PREFIX'/share/doc/" + APPNAME + ']')

    if parallel_debug:
        opt.load('parallel_debug')

def add_cflag(conf, flag):
    conf.env.prepend_value('CFLAGS', flag)

def add_linkflag(conf, flag):
    conf.env.prepend_value('LINKFLAGS', flag)

def check_gcc_optimizations_enabled(flags):
    gcc_optimizations_enabled = False
    for flag in flags:
        if len(flag) < 2 or flag[0] != '-' or flag[1] != 'O':
            continue
        if len(flag) == 2:
            gcc_optimizations_enabled = True;
        else:
            gcc_optimizations_enabled = flag[2] != '0';
    return gcc_optimizations_enabled

def configure(conf):
    conf.load('compiler_c')
    if parallel_debug:
        conf.load('parallel_debug')

    conf.check_cfg(
        package = 'dbus-1',
        atleast_version = '1.0.0',
        mandatory = True,
        errmsg = "not installed, see http://dbus.freedesktop.org/",
        args = '--cflags --libs')

    if Options.options.libdir:
        conf.env['LIBDIR'] = Options.options.libdir
    else:
        conf.env['LIBDIR'] = os.path.join(os.path.normpath(conf.env['PREFIX']), 'lib')

    if Options.options.docdir:
        conf.env['DOCDIR'] = Options.options.docdir
    else:
        conf.env['DOCDIR'] = os.path.join(os.path.normpath(conf.env['PREFIX']), 'share', 'doc', APPNAME)

    #conf.env['BUILD_DOXYGEN_DOCS'] = Options.options.doxygen

    add_cflag(conf, '-fvisibility=hidden')

    conf.env['BUILD_WERROR'] = Options.options.debug
    add_cflag(conf, '-Wall')
    add_cflag(conf, '-Wextra')
    #conf.env.append_unique('CXXFLAGS', '-Wno-unused-parameter') # the UNUSED() macro doesnt work for C++

    #add_cflag(conf, '-Wimplicit-fallthrough=2')

    if conf.env['BUILD_WERROR']:
        add_cflag(conf, '-Werror')

    conf.env['BUILD_DEBUG'] = Options.options.debug
    if conf.env['BUILD_DEBUG']:
        add_cflag(conf, '-g')
        add_cflag(conf, '-O0')
        add_linkflag(conf, '-g')
    else:
        add_cflag(conf, '-O1')

    conf.env['DATA_DIR'] = os.path.normpath(os.path.join(conf.env['PREFIX'], 'share', APPNAME))
    conf.env['LOCALE_DIR'] = os.path.normpath(os.path.join(conf.env['PREFIX'], 'share', 'locale'))

    # write some parts of the configure environment to the config.h file
    conf.define('DATA_DIR', conf.env['DATA_DIR'])
    conf.define('LOCALE_DIR', conf.env['LOCALE_DIR'])
    conf.define('PACKAGE_VERSION', VERSION)
    conf.define('_GNU_SOURCE', 1)
    conf.write_config_header('config.h')

    display_msg(conf)

    display_msg(conf, "==================")
    version_msg = APPNAME + "-" + VERSION

    if os.access('version.h', os.R_OK):
        data = open('version.h').read()
        m = re.match(r'^#define CDBUS_GIT_VERSION "([^"]*)"$', data)
        if m != None:
            version_msg += " exported from " + m.group(1)
    elif os.access('.git', os.R_OK):
        version_msg += " git revision will checked and eventually updated during build"

    display_msg(conf, version_msg)

    display_msg(conf)
    display_msg(conf, "Install prefix", conf.env['PREFIX'], 'CYAN')

    #display_msg(conf, 'Treat warnings as errors', yesno(conf.env['BUILD_WERROR']))
    display_msg(conf, 'Debuggable binaries', yesno(conf.env['BUILD_DEBUG']))
    #display_msg(conf, 'Build doxygen documentation', yesno(conf.env['BUILD_DOXYGEN_DOCS']))

    display_msg(conf, 'C compiler flags', repr(conf.env['CFLAGS']))

    display_msg(conf)

def git_ver(self):
    bld = self.generator.bld
    header = self.outputs[0].abspath()
    if os.access('./version.h', os.R_OK):
        header = os.path.join(os.getcwd(), out, "version.h")
        shutil.copy('./version.h', header)
        data = open(header).read()
        m = re.match(r'^#define CDBUS_GIT_VERSION "([^"]*)"$', data)
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
    fi.write('#define CDBUS_GIT_VERSION "%s"\n' % self.ver)
    fi.close()

def build(bld):
    if not bld.env['DATA_DIR']:
        raise "DATA_DIR is emtpy"

    bld(rule=git_ver, target='version.h', update_outputs=True, always=True, ext_out=['.h'])

    libcdbus = bld.shlib(source = [], features = 'c cshlib', includes = [bld.path.get_bld(), "./include/cdbus-1"])
    libcdbus.uselib = 'DBUS-1'
    libcdbus.target = 'cdbus'
    libcdbus.vnum = "1.0.0"
    libcdbus.defines = ['LOG_OUTPUT_STDOUT']

    libcdbus.source = [
        'src/signal.c',
        'src/method.c',
        'src/object_path.c',
        'src/interface.c',
        'src/helpers.c',
        'src/log.c',
    ]

    bld.install_files('${PREFIX}/include/cdbus-1/cdbus', [
        'version.h',
        'include/cdbus-1/cdbus/signal.h',
        'include/cdbus-1/cdbus/method.h',
        'include/cdbus-1/cdbus/object_path.h',
        'include/cdbus-1/cdbus/interface.h',
        'include/cdbus-1/cdbus/cdbus.h',
        'include/cdbus-1/cdbus/log.h',
    ])

    # process cdbus-1.pc.in -> cdbus-1.pc
    bld(
        features     = 'subst', # the feature 'subst' overrides the source/target processing
        source       = 'cdbus-1.pc.in', # list of string or nodes
        target       = 'cdbus-1.pc', # list of strings or nodes
        install_path = '${LIBDIR}/pkgconfig/',
        # variables to use in the substitution
        prefix       = bld.env['PREFIX'],
        libdir       = bld.env['LIBDIR'],
        includedir   = os.path.normpath(bld.env['PREFIX'] + '/include'))

    bld.install_files('${DOCDIR}', [
        "NEWS.adoc",
        "README.adoc",
        "doc/gpl2.txt",
        "AUTHORS",
    ])

    # if bld.env['BUILD_DOXYGEN_DOCS'] == True:
    #     html_docs_source_dir = "build/default/html"
    #     if bld.cmd == 'clean':
    #         if os.access(html_docs_source_dir, os.R_OK):
    #             Logs.pprint('CYAN', "Removing doxygen generated documentation...")
    #             shutil.rmtree(html_docs_source_dir)
    #             Logs.pprint('CYAN', "Removing doxygen generated documentation done.")
    #     elif bld.cmd == 'build':
    #         if not os.access(html_docs_source_dir, os.R_OK):
    #             os.popen("doxygen").read()
    #         else:
    #             Logs.pprint('CYAN', "doxygen documentation already built.")
