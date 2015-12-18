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
AC_INIT([dupdate], esyscmd([./version.sh]), [eha@deif.com])
AC_CONFIG_SRCDIR([src/])
AC_CONFIG_HEADERS([config.h])
AC_CANONICAL_SYSTEM
AC_USE_SYSTEM_EXTENSIONS

# Automake
AM_INIT_AUTOMAKE([foreign])

# Build options

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h sys/param.h syslog.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_STRNLEN
AC_CHECK_FUNCS([strdup strerror strtoul])

AM_CONDITIONAL([DAEMON], [test "$ac_cv_func_fork" = yes])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
