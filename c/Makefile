HC_VERS = 1.14
CHANGEME1 = 1 # put PACKAGER = EBUILD|RPM etc here
CHANGEME2 = 2
CHANGEME3 = 3
INSTALL_PATH = /usr/bin
MAN_INSTALL_PATH = /usr/share/man/man1
DOC_INSTALL_PATH = /usr/share/doc/hashcash-$(HC_VERS)
MAKEDEPEND = makedepend
MSLIB = mslib 
# here you can choose the regexp style your system has
# default is POSIX
# 	CFLAGS = -DREGEXP_POSIX
# if no POSIX regexp support, try BSD
# 	CFLAGS = -DREGEXP_BSD
# if no POSIX or BSD, disable, still have builtin basic wildcard support
# 	CFLAGS = 
COPT_GENERIC = -O3
COPT_GNU = -O3 -funroll-loops
COPT_X86 = -O3 -funroll-loops -march=i386 -mcpu=pentium -mmmx \
	-D_REENTRANT -D_THREAD_SAFE -fPIC
COPT_MINGW = -O3 -funroll-loops -march=i386 -mcpu=pentium -mmmx \
        -D_REENTRANT -D_THREAD_SAFE
COPT_G3_OSX = -O3 -funroll-loops -fno-inline -mcpu=750 -faltivec
COPT_PPC_LINUX = -O3 -funroll-loops -fno-inline -mcpu=604e -maltivec \
	-mabi=altivec
CFLAGS = -DREGEXP_POSIX
EXES = hashcash$(EXE) sha1$(EXE) sha1test$(EXE)
INSTALL = install
POD2MAN = pod2man
POD2HTML = pod2html
POD2TEXT = pod2text
DELETE = rm -f
ETAGS = etags
FASTLIBS = libfastmint.o fastmint_mmx_standard_1.o fastmint_mmx_compact_1.o \
	fastmint_ansi_compact_1.o fastmint_ansi_standard_1.o \
	fastmint_ansi_compact_2.o fastmint_ansi_standard_2.o \
	fastmint_altivec_standard_1.o fastmint_altivec_standard_2.o \
	fastmint_altivec_compact_2.o fastmint_ansi_ultracompact_1.o
OBJS = libsha1.o libhc.o sdb.o lock.o utct.o random.o sstring.o \
	getopt.o $(FASTLIBS)
DLLOBJS = libhc.o libsha1.o utct.o sdb.o lock.o sstring.o random.o $(FASTLIBS)
EXEOBJS = hashcash.o

DIST = ../dist.csh

default:	help generic

help:
	@echo "make <platform> where platform is:"
	@echo "    x86, mingw, mingw-dll, g3-osx, ppc-linux, gnu, generic"
	@echo "other make targets are docs, install, clean, distclean, docclean"
	@echo ""
	@echo "(doing make generic by default)"
	@echo ""

generic:
	$(MAKE) "CFLAGS=$(CFLAGS) $(COPT_GENERIC) $(COPT)" build

gnu:
	$(MAKE) "CFLAGS=$(CFLAGS) $(COPT_GNU) $(COPT)" "CC=gcc" build

x86: 
	$(MAKE) "CFLAGS=$(CFLAGS) $(COPT_X86) $(COPT)" build

mingw: 
	$(MAKE) "CC=gcc" "EXE=.exe" "CFLAGS=$(COPT_MINGW) -DMONOLITHIC $(COPT)" build

mingw-dll:
	$(MAKE) "CC=gcc" "EXE=.exe" "CFLAGS=$(COPT_MINGW) $(COPT)" build-dll

g3-osx:
	$(MAKE) "CFLAGS=$(CFLAGS) $(COPT_G3_OSX) $(COPT)" build

ppc-linux:
	$(MAKE) "CFLAGS=$(CFLAGS) $(COPT_PPC_LINUX) $(COPT)" build

build:	hashcash$(EXE) sha1$(EXE)

build-dll:      hashcash-dll$(EXE) sha1$(EXE)

hashcash$(EXE):	hashcash.o $(OBJS) 
	$(CC) $(LDFLAGS) hashcash.o $(OBJS) -o $@

sha1$(EXE):	sha1.o libsha1.o
	$(CC) $(LDFLAGS) sha1.o libsha1.o -o $@

example$(EXE):	example.o $(OBJS)
	$(CC) $(LDFLAGS) example.o $(OBJS) -o $@

hashcash-dll$(EXE):   $(EXEOBJS) hashcash.dll
	$(CC) $(LDFLAGS) $(EXEOBJS) hashcash.dll -o $@

sha1test$(EXE):	sha1test.o libsha1.o
	$(CC) $(LDFLAGS) sha1test.o libsha1.o -o $@

all:	$(EXES)

hashcash.dll:   $(DLLOBJS)
	$(CC) -shared -o hashcash.dll $(DLLOBJS) \
	-Wl,--output-def,hashcash.def,--out-implib,libhashcash.a
	$(MSLIB) /machine:x86 /def:hashcash.def

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
	$(DELETE) *.o *~ $(EXES) hashcash-dll.* *.db *.bak TAGS core* 
	$(DELETE) *.bak test/* *.dll *.lib *.exe *.a

tags:
	$(ETAGS) *.c *.h

dist:	
	$(DIST)

# DO NOT DELETE

example.o: sstring.h sdb.h hashcash.h getopt.h
fastmint_altivec_compact_2.o: libfastmint.h hashcash.h
fastmint_altivec_standard_1.o: libfastmint.h hashcash.h
fastmint_altivec_standard_2.o: libfastmint.h hashcash.h
fastmint_ansi_compact_1.o: libfastmint.h hashcash.h
fastmint_ansi_compact_2.o: libfastmint.h hashcash.h
fastmint_ansi_standard_1.o: libfastmint.h hashcash.h
fastmint_ansi_standard_2.o: libfastmint.h hashcash.h
fastmint_ansi_ultracompact_1.o: libfastmint.h hashcash.h
fastmint_mmx_compact_1.o: libfastmint.h hashcash.h
fastmint_mmx_standard_1.o: libfastmint.h hashcash.h
getopt.o: getopt.h
hashcash.o: sdb.h utct.h random.h hashcash.h libfastmint.h sstring.h getopt.h
hashcash.o: sha1.h types.h
libfastmint.o: random.h sha1.h types.h libfastmint.h hashcash.h
libhc.o: hashcash.h utct.h libfastmint.h sha1.h types.h random.h sstring.h
libsha1.o: sha1.h types.h
lock.o: lock.h
random.o: random.h sha1.h types.h
sdb.o: types.h lock.h sdb.h utct.h
sha1.o: sha1.h types.h
sha1test.o: sha1.h types.h
sstring.o: sstring.h
utct.o: sstring.h utct.h
libfastmint.o: hashcash.h
sha1.o: types.h
