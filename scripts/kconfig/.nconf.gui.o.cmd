cmd_scripts/kconfig/nconf.gui.o := gcc -Wp,-MD,scripts/kconfig/.nconf.gui.o.d -W -Wno-unused-parameter -Wno-missing-field-initializers -fno-delete-null-pointer-checks  -Wno-empty-body   -D_GNU_SOURCE  -DCURSES_LOC="<ncurses.h>" -DLOCALE   -c -o scripts/kconfig/nconf.gui.o scripts/kconfig/nconf.gui.c

source_scripts/kconfig/nconf.gui.o := scripts/kconfig/nconf.gui.c

deps_scripts/kconfig/nconf.gui.o := \
  /usr/include/stdc-predef.h \
  scripts/kconfig/nconf.h \
  /usr/include/ctype.h \
  /usr/include/features.h \
  /usr/include/x86_64-linux-gnu/sys/cdefs.h \
  /usr/include/x86_64-linux-gnu/bits/wordsize.h \
  /usr/include/x86_64-linux-gnu/gnu/stubs.h \
  /usr/include/x86_64-linux-gnu/gnu/stubs-64.h \
  /usr/include/x86_64-linux-gnu/bits/types.h \
  /usr/include/x86_64-linux-gnu/bits/typesizes.h \
  /usr/include/endian.h \
  /usr/include/x86_64-linux-gnu/bits/endian.h \
  /usr/include/x86_64-linux-gnu/bits/byteswap.h \
  /usr/include/x86_64-linux-gnu/bits/byteswap-16.h \
  /usr/include/xlocale.h \
  /usr/include/errno.h \
  /usr/include/x86_64-linux-gnu/bits/errno.h \
  /usr/include/linux/errno.h \
  /usr/include/x86_64-linux-gnu/asm/errno.h \
  /usr/include/asm-generic/errno.h \
  /usr/include/asm-generic/errno-base.h \
  /usr/include/fcntl.h \
  /usr/include/x86_64-linux-gnu/bits/fcntl.h \
  /usr/include/x86_64-linux-gnu/bits/fcntl-linux.h \
  /usr/include/x86_64-linux-gnu/bits/uio.h \
  /usr/include/x86_64-linux-gnu/sys/types.h \
  /usr/include/time.h \
  /usr/lib/gcc/x86_64-linux-gnu/6/include/stddef.h \
  /usr/include/x86_64-linux-gnu/sys/select.h \
  /usr/include/x86_64-linux-gnu/bits/select.h \
  /usr/include/x86_64-linux-gnu/bits/sigset.h \
  /usr/include/x86_64-linux-gnu/bits/time.h \
  /usr/include/x86_64-linux-gnu/sys/sysmacros.h \
  /usr/include/x86_64-linux-gnu/bits/pthreadtypes.h \
  /usr/include/x86_64-linux-gnu/bits/stat.h \
  /usr/lib/gcc/x86_64-linux-gnu/6/include-fixed/limits.h \
  /usr/lib/gcc/x86_64-linux-gnu/6/include-fixed/syslimits.h \
  /usr/include/limits.h \
  /usr/include/x86_64-linux-gnu/bits/posix1_lim.h \
  /usr/include/x86_64-linux-gnu/bits/local_lim.h \
  /usr/include/linux/limits.h \
  /usr/include/x86_64-linux-gnu/bits/posix2_lim.h \
  /usr/include/x86_64-linux-gnu/bits/xopen_lim.h \
  /usr/include/x86_64-linux-gnu/bits/stdio_lim.h \
  /usr/lib/gcc/x86_64-linux-gnu/6/include/stdarg.h \
  /usr/include/stdlib.h \
  /usr/include/x86_64-linux-gnu/bits/waitflags.h \
  /usr/include/x86_64-linux-gnu/bits/waitstatus.h \
  /usr/include/alloca.h \
  /usr/include/x86_64-linux-gnu/bits/stdlib-float.h \
  /usr/include/string.h \
  /usr/include/unistd.h \
  /usr/include/x86_64-linux-gnu/bits/posix_opt.h \
  /usr/include/x86_64-linux-gnu/bits/environments.h \
  /usr/include/x86_64-linux-gnu/bits/confname.h \
  /usr/include/getopt.h \
  /usr/include/locale.h \
  /usr/include/x86_64-linux-gnu/bits/locale.h \
  /usr/include/curses.h \
  /usr/include/ncurses_dll.h \
  /usr/lib/gcc/x86_64-linux-gnu/6/include/stdint.h \
  /usr/include/stdint.h \
  /usr/include/x86_64-linux-gnu/bits/wchar.h \
  /usr/include/stdio.h \
  /usr/include/libio.h \
  /usr/include/_G_config.h \
  /usr/include/wchar.h \
  /usr/include/x86_64-linux-gnu/bits/sys_errlist.h \
  /usr/lib/gcc/x86_64-linux-gnu/6/include/stdbool.h \
  /usr/include/unctrl.h \
  /usr/include/menu.h \
  /usr/include/eti.h \
  /usr/include/panel.h \
  /usr/include/form.h \
  /usr/include/x86_64-linux-gnu/bits/timex.h \
  /usr/include/x86_64-linux-gnu/sys/time.h \
  /usr/include/curses.h \

scripts/kconfig/nconf.gui.o: $(deps_scripts/kconfig/nconf.gui.o)

$(deps_scripts/kconfig/nconf.gui.o):
