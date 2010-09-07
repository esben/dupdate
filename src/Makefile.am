sbin_PROGRAMS = dupdate dboot
dupdate_SOURCES = dupdate.c
dboot_SOURCES = dboot.c dboot_list.c
if MTD
dboot_SOURCES += dboot_mtd.c
endif
if NVRAM
dboot_SOURCES += dboot_nvram.c
endif
