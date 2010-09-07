#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# This file is part of dupdate.
#
# dupdate is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# dupdate is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with dupdate.  If not, see <http://www.gnu.org/licenses/>.

AC_PREREQ([2.64])
AC_INIT([dscan], esyscmd([./version.sh]), [eha@doredevelopment.dk])
AC_CONFIG_SRCDIR([src/dboot.c])
AC_CONFIG_HEADERS([config.h])
AC_CANONICAL_SYSTEM
AC_USE_SYSTEM_EXTENSIONS

# Automake
AM_INIT_AUTOMAKE

# Build options
AC_ARG_WITH(backend,
  AS_HELP_STRING([--with-backend=mtd|nvram],
    [choose if dboot status should be stored in mtd (NOR) or NVRAM]))
AM_CONDITIONAL(MTD, test x$with_backend = xmtd)
AM_CONDITIONAL(NVRAM, test x$with_backend = xnvram)

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h stdint.h stdlib.h string.h sys/ioctl.h sys/mount.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_CHECK_FUNCS([getcwd realpath strerror strstr])

AC_CONFIG_FILES([Makefile
		 src/Makefile])
AC_OUTPUT
