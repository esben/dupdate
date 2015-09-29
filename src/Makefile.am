bin_PROGRAMS = dupdate inotifyd simple_cmp
bin_SCRIPTS = dupdate-inotifyd-agent

dupdate_SOURCES = dupdate.c common.c

inotifyd_SOURCES = inotifyd.c common.c
if DAEMON
inotifyd_SOURCES += daemon.c
endif

simple_cmp_SOURCES = simple_cmp.c
