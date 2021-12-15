


sbin_PROGRAMS = momctl

momctl_CPPFLAGS = \
	-I$(top_srcdir)/src/include \
	@libz_inc@ \
	@KRB5_CFLAGS@

momctl_LDADD = \
	$(top_builddir)/src/lib/Libtpp/libtpp.a \
	$(top_builddir)/src/lib/Liblog/liblog.a \
	$(top_builddir)/src/lib/Libnet/libnet.a \
	$(top_builddir)/src/lib/Libpbs/.libs/libpbs.a \
	$(top_builddir)/src/lib/Libutil/libutil.a \
	@KRB5_LIBS@ \
	-lcrypto \
	-lpthread \
	@libz_lib@

momctl_SOURCES = \
	momctl.c

