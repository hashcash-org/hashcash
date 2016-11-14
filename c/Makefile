HC_VERS = 1.00
INSTALL_PATH = /usr/bin
MAN_INSTALL_PATH = /usr/share/man/man1
DOC_INSTALL_PATH = /usr/share/doc/hashcash-$(HC_VERS)
MAKEDEPEND = makedepend
# add -DCHROMATIX for libfastmint integration
EXTRA_COPT = -O3 -funroll-loops
COPT = -O $(EXTRA_COPT)
OPT = -DREGEXP_POSIX
# if no POSIX regexp support, try BSD
# OPT = -DREGEXP_BSD
# if no POSIX or BSD, disable, still have builtin basic wildcard support
# OPT = -O
LDFLAGS = $(CARCH)
CFLAGS = $(COPT) $(OPT) $(CWARN) $(CARCH)
EXES = hashcash sha1 sha1test fastmint_benchtest
INSTALL = /usr/bin/install
POD2MAN = pod2man
POD2HTML = pod2html
POD2TEXT = pod2text
DELETE = /bin/rm -f
ETAGS = etags
# FASTLIBS = libfastmint.o fastmint_mmx_standard_1.o fastmint_mmx_compact_1.o \
#	fastmint_ansi_compact_1.o fastmint_ansi_standard_1.o \
#	fastmint_ansi_compact_2.o fastmint_ansi_standard_2.o \
#	fastmint_altivec_standard_1.o fastmint_altivec_standard_2.o \
#	fastmint_altivec_compact_2.o fastmint_ansi_ultracompact_1.o
OBJS = hashcash.o libsha1.o libhc.o sdb.o utct.o random.o lock.o sstring.o
# $(FASTLIBS)
# FASTOBJS = fastmint_benchtest.o libsha1.o random.o $(FASTLIBS)

DIST = ../dist.csh

build:	hashcash sha1 #fastmint_benchtest

hashcash:	$(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

sha1:	sha1.o libsha1.o
	$(CC) $(LDFLAGS) sha1.o libsha1.o -o $@

sha1test:	sha1test.o libsha1.o 
	$(CC) $(LDFLAGS) sha1test.o libsha1.o -o $@

# fastmint_benchtest: $(FASTOBJS)
# 	$(CC) $(LDFLAGS) $(FASTOBJS) -o $@

all:	$(EXES)

docs:	hashcash.1 hashcash.html hashcash.txt sha1-hashcash.1 \
sha1-hashcash.html sha1-hashcash.txt

hashcash.1:	hashcash.pod
	$(POD2MAN) -s 1 -c hashcash -r $(HC_VERS) $? > $@

hashcash.html:	hashcash.pod
	$(POD2HTML) --title hashcash $? > $@
	$(DELETE) pod2htm*

hashcash.txt: hashcash.pod
	$(POD2TEXT) $? > $@

sha1-hashcash.1:	sha1-hashcash.pod
	$(POD2MAN) -s 1 -c sha1 -r $(HC_VERS) $? > $@

sha1-hashcash.html:	sha1-hashcash.pod
	$(POD2HTML) --title sha1 $? > $@
	$(DELETE) pod2htm*

sha1-hashcash.txt: sha1-hashcash.pod
	$(POD2TEXT) $? > $@

install:	hashcash sha1 hashcash.1 sha1-hashcash.1
	$(INSTALL) -d $(INSTALL_PATH)
	$(INSTALL) hashcash sha1 $(INSTALL_PATH)
	$(INSTALL) -d $(MAN_INSTALL_PATH)
	$(INSTALL) -m 644 hashcash.1 sha1-hashcash.1 $(MAN_INSTALL_PATH)
	$(INSTALL) -d $(DOC_INSTALL_PATH)
	$(INSTALL) -m 644 README LICENSE CHANGELOG $(DOC_INSTALL_PATH)

depend:
	$(MAKEDEPEND) -- -Y *.c *.h

docclean:
	$(DELETE) hashcash.txt hashcash.1 hashcash.html pod2htm*
	$(DELETE) sha1-hashcash.txt sha1-hashcash.1 sha1-hashcash.html

clean:
	$(DELETE) *.o *~

distclean:
	$(DELETE) *.o *~ $(EXES) *.db *.bak TAGS core* *.bak test/*

tags:
	$(ETAGS) *.c *.h

dist:	
	$(DIST)

# DO NOT DELETE

fastmint_altivec_compact_2.o: libfastmint.h
fastmint_altivec_standard_1.o: libfastmint.h
fastmint_altivec_standard_2.o: libfastmint.h
fastmint_ansi_compact_1.o: libfastmint.h
fastmint_ansi_compact_2.o: libfastmint.h
fastmint_ansi_standard_1.o: libfastmint.h
fastmint_ansi_standard_2.o: libfastmint.h
fastmint_benchtest.o: libfastmint.h
fastmint_mmx_compact_1.o: libfastmint.h
fastmint_mmx_standard_1.o: libfastmint.h
getopt.o: getopt.h
gmp.o: gmp.h
hashcash.o: sdb.h utct.h random.h types.h hashcash.h sstring.h sha1.h
libfastmint.o: libfastmint.h random.h sha1.h types.h
libhc.o: hashcash.h utct.h types.h sha1.h random.h sstring.h
libsha1.o: sha1.h types.h
lock.o: lock.h
random.o: random.h types.h sha1.h
sdb.o: sdb.h types.h utct.h lock.h
sha1.o: sha1.h types.h
sha1test.o: sha1.h types.h
sstring.o: sstring.h
utct.o: utct.h sstring.h
hashcash.o: utct.h types.h
sha1.o: types.h
