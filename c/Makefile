HC_VERS = 1.04
CHANGEME1 = 1 # put PACKAGER = EBUILD|RPM etc here
CHANGEME2 = 2
CHANGEME3 = 3
ifdef CFLAGS
ifndef PACKAGER
PACKAGER = MANUAL
endif
endif
ifdef COPT
ifndef PACKAGER
PACKAGER = MANUAL
endif
endif
INSTALL_PATH = /usr/bin
MAN_INSTALL_PATH = /usr/share/man/man1
DOC_INSTALL_PATH = /usr/share/doc/hashcash-$(HC_VERS)
MAKEDEPEND = makedepend
# here you can choose the regexp style your system has
# default is POSIX
# 	CFLAGS = -DREGEXP_POSIX
# if no POSIX regexp support, try BSD
# 	CFLAGS = -DREGEXP_BSD
# if no POSIX or BSD, disable, still have builtin basic wildcard support
# 	CFLAGS = 
COPT_GENERIC = -O3 -funroll-loops
COPT_X86 = -O3 -funroll-loops -march=i386 -mcpu=pentium -mmmx
COPT_G3_OSX = -O3 -funroll-loops -fno-inline -mcpu=750 -faltivec
COPT_PPC_LINUX = -O3 -funroll-loops -fno-inline -mcpu=604e -maltivec \
	-mabi=altivec
CFLAGS = -DREGEXP_POSIX
EXES = hashcash sha1 sha1test
INSTALL = /usr/bin/install
POD2MAN = pod2man
POD2HTML = pod2html
POD2TEXT = pod2text
DELETE = /bin/rm -f
ETAGS = etags
FASTLIBS = libfastmint.o fastmint_mmx_standard_1.o fastmint_mmx_compact_1.o \
	fastmint_ansi_compact_1.o fastmint_ansi_standard_1.o \
	fastmint_ansi_compact_2.o fastmint_ansi_standard_2.o \
	fastmint_altivec_standard_1.o fastmint_altivec_standard_2.o \
	fastmint_altivec_compact_2.o fastmint_ansi_ultracompact_1.o
OBJS = hashcash.o libsha1.o libhc.o sdb.o utct.o random.o lock.o sstring.o \
	getopt.o $(FASTLIBS)

DIST = ../dist.csh

ifndef PACKAGER
help:
	@echo "make <platform> where platform is x86, g3-osx, ppc-linux, generic"
	@echo "other make targets are docs, install, clean, distclean, docclean"
endif

generic:
	$(MAKE) "CFLAGS=$(CFLAGS) $(COPT_GENERIC) $(COPT)" build

x86: 
	$(MAKE) "CFLAGS=$(CFLAGS) $(COPT_X86) $(COPT)" build

g3-osx:
	$(MAKE) "CFLAGS=$(CFLAGS) $(COPT_G3_OSX) $(COPT)" build

ppc-linux:
	$(MAKE) "CFLAGS=$(CFLAGS) $(COPT_PPC_LINUX) $(COPT)" build

build:	hashcash sha1

hashcash:	$(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

sha1:	sha1.o libsha1.o
	$(CC) $(LDFLAGS) sha1.o libsha1.o -o $@

sha1test:	sha1test.o libsha1.o 
	$(CC) $(LDFLAGS) sha1test.o libsha1.o -o $@

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
fastmint_ansi_ultracompact_1.o: libfastmint.h
fastmint_mmx_compact_1.o: libfastmint.h
fastmint_mmx_standard_1.o: libfastmint.h
getopt.o: getopt.h
hashcash.o: sdb.h utct.h random.h hashcash.h types.h sstring.h getopt.h
hashcash.o: sha1.h
libfastmint.o: libfastmint.h random.h sha1.h types.h
libhc.o: hashcash.h utct.h types.h sha1.h random.h sstring.h
libsha1.o: sha1.h types.h
lock.o: lock.h
random.o: random.h sha1.h types.h
sdb.o: sdb.h types.h utct.h lock.h
sha1.o: sha1.h types.h
sha1test.o: sha1.h types.h
sstring.o: sstring.h
utct.o: utct.h sstring.h
hashcash.o: utct.h types.h
sha1.o: types.h
