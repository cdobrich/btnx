#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(btnx, 0.4.11, oasalone@gmail.com)
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_SRCDIR([src/btnx.h])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
AC_GNU_SOURCE

# Checks for libraries.
PKG_CHECK_MODULES(LIBDAEMON, libdaemon >= 0.10)

# Check command line arguments
AC_ARG_VAR(init_scripts_path, The init script directory for your system. Default is "/etc/init.d". Do not append a final '/'.)
AC_ARG_VAR(config_path, The path to btnx configuration files. Default is "/etc/btnx". Do not append a final '/'.)
AC_ARG_VAR(init_tool, Controls whether installation attempts to register the btnx init script. Set to "no" to disable. Otherwise, configure autodetects.)
AC_ARG_VAR(output_syslog, Set to "yes" to redirect btnx output to syslog when started with init script. Default is "no".)

test -z "$init_scripts_path" && init_scripts_path=/etc/init.d
test -z "$config_path" && config_path=/etc/btnx

# Series of checks to see whether init script should be installed and how
# -----------------------------------------------------------------------
if test -z "$init_tool"; then
	AC_CHECK_PROGS(init_tool, update-rc.d chkconfig rc-update, none, $PATH:/sbin:/usr/sbin:/usr/local/sbin)
	if test x$init_tool = xupdate-rc.d; then
		init_add_cmd="update-rc.d btnx start 49 2 3 4 5 . stop 49 0 1 6 ."
		init_del_cmd="update-rc.d -f btnx remove"
		install_init=1
	elif test x$init_tool = xchkconfig; then
		init_add_cmd="chkconfig --add btnx"
		init_del_cmd="chkconfig --del btnx"
		install_init=1
	elif test x$init_tool = xrc-update; then
		init_add_cmd="rc-update add btnx default"
		init_del_cmd="rc-update del btnx default"
		install_init=1
	else
		install_init=0
	fi
else
	install_init=0
fi
AC_SUBST(install_init)
AM_CONDITIONAL(INSTALL_INIT_TEST, test x$install_init = x1)
AC_SUBST(init_add_cmd)
AC_SUBST(init_del_cmd)
# -----------------------------------------------------------------------

if test -z "$output_syslog" || test x$output_syslog = xno; then
	arg_syslog=
elif test x$output_syslog = xyes; then
	arg_syslog=-l
else
	AC_MSG_ERROR(Invalid value supplied for output_syslog)
fi

initdir=$init_scripts_path
configdir=$config_path
AC_SUBST(initdir)
AC_SUBST(configdir)
AC_SUBST(arg_syslog)
AC_DEFINE_UNQUOTED(CONFIG_PATH, ["$config_path"], [Path to btnx configurations])

#### STILL NEED CHECKING: cat and grep ####
# Check for su
AC_PATH_PROG(SU_PATH, su, "/bin/su")
AC_DEFINE_UNQUOTED(SU_PATH, ["$SU_PATH"], [su path])

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/file.h sys/ioctl.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_HEADER_TIME

# Checks for library functions.
AC_FUNC_FORK
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([ftruncate gettimeofday memset select strcasecmp strchr strerror strtol])

AC_CONFIG_FILES([Makefile src/Makefile data/Makefile])

if test -z "$init_tool" && test x$install_init = x0; then
	AC_MSG_WARN(Configure did not find suitable tools to install and register the init script. btnx init script will NOT be installed.)
fi

AC_OUTPUT
