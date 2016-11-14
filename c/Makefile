HC_VERS = 1.22
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
# 	REGEXP = -DREGEXP_POSIX
# if no POSIX regexp support, try BSD
# 	REGEXP = -DREGEXP_BSD
# if no POSIX or BSD, disable, still have builtin basic wildcard support
# 	REGEXP = 
REGEXP=-DREGEXP_POSIX
COPT_DEBUG = -g -DDEBUG
COPT_GENERIC = -O3
COPT_GNU = -O3 -funroll-loops
COPT_X86 = -O3 -funroll-loops -march=pentium-mmx -mmmx \
	-D_REENTRANT -D_THREAD_SAFE -fPIC
COPT_MINGW = -O3 -funroll-loops -march=pentium-mmx -mmmx \
        -D_REENTRANT -D_THREAD_SAFE
COPT_G3_OSX = -O3 -funroll-loops -fno-inline -mcpu=750 -faltivec
COPT_PPC_LINUX = -O3 -funroll-loops -fno-inline -mcpu=604e -maltivec \
	-mabi=altivec
LIB=.a
# request static link of -lcrypto only
LIBCRYPTO=/usr/lib/libcrypto.a
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
	fastmint_altivec_compact_2.o fastmint_ansi_ultracompact_1.o \
	fastmint_library.o
OBJS = libsha1.o libhc.o sdb.o lock.o utct.o random.o sstring.o \
	getopt.o $(FASTLIBS)
LIBOBJS = libhc.o libsha1.o utct.o sdb.o lock.o sstring.o random.o $(FASTLIBS)
EXEOBJS = hashcash.o

DIST = ../dist.csh

default:	help generic

help:
	@echo "make <platform> where platform is:"
	@echo "    x86, mingw, mingw-dll, g3-osx, ppc-linux, gnu, generic, debug"
	@echo "or to link with openSSL for SHA1 rather than builtin:"
	@echo "    x86-openssl, g3-osx-openssl, ppc-linux-openssl, "
	@echo "    gnu-openssl, generic-openssl, debug-openssl"
	@echo "other make targets are docs, install, clean, distclean, docclean"
	@echo ""
	@echo "(doing make generic by default)"
	@echo ""

generic:
	$(MAKE) "CFLAGS=$(CFLAGS) $(REGEXP) $(COPT_GENERIC) $(COPT)" build

debug:
	$(MAKE) "CFLAGS=$(CFLAGS) $(REGEXP) $(COPT_DEBUG) $(COPT)" build

gnu:
	$(MAKE) "CFLAGS=$(CFLAGS) $(REGEXP) $(COPT_GNU) $(COPT)" "CC=gcc" build

x86: 
	$(MAKE) "CFLAGS=$(CFLAGS) $(REGEXP) $(COPT_X86) $(COPT)" build

g3-osx:
	$(MAKE) "CFLAGS=$(CFLAGS) $(REGEXP) $(COPT_G3_OSX) $(COPT)" build

ppc-linux:
	$(MAKE) "CFLAGS=$(CFLAGS) $(REGEXP) $(COPT_PPC_LINUX) $(COPT)" build

# mingw windows targets (cross compiler, or native)

mingw:
	$(MAKE) "LIB=.lib" "CC=gcc" "EXE=.exe" "CFLAGS=$(COPT_MINGW) -DMONOLITHIC $(COPT)" build

mingw-dll:
	$(MAKE) "CC=gcc" "EXE=.exe" "CFLAGS=$(COPT_MINGW) $(COPT)" build-dll


# openSSL versions of targets

x86-openssl:
	$(MAKE) x86 "CFLAGS=$(CFLAGS) -DOPENSSL" "LDFLAGS=$(LDFLAGS) $(LIBCRYPTO)"

g3-osx-openssl:
	$(MAKE) g3-osx "CFLAGS=$(CFLAGS) -DOPENSSL" "LDFLAGS=$(LDFLAGS) $(LIBCRYPTO)"

ppc-linux-openssl:
	$(MAKE) ppc-linux "CFLAGS=$(CFLAGS) -DOPENSSL" "LDFLAGS=$(LDFLAGS) $(LIBCRYPTO)"

gnu-openssl:
	$(MAKE) gnu "CFLAGS=$(CFLAGS) -DOPENSSL" "LDFLAGS=$(LDFLAGS) $(LIBCRYPTO)"

generic-openssl:
	$(MAKE) generic "CFLAGS=$(CFLAGS) -DOPENSSL" "LDFLAGS=$(LDFLAGS) $(LIBCRYPTO)"

debug-openssl:
	$(MAKE) debug "CFLAGS=$(CFLAGS) -DOPENSSL" "LDFLAGS=$(LDFLAGS) $(LIBCRYPTO)"


build:	hashcash$(EXE) sha1$(EXE)

build-dll:      hashcash-dll$(EXE) sha1$(EXE)

hashcash$(EXE):	hashcash.o getopt.o libhashcash$(LIB) 
	$(CC) hashcash.o getopt.o libhashcash$(LIB) -o $@ $(LDFLAGS)

sha1$(EXE):	sha1.o libsha1.o
	$(CC) sha1.o libsha1.o -o $@ $(LDFLAGS)

example$(EXE):	example.o getopt.o libhashcash$(LIB)
	$(CC) example.o getopt.o libhashcash$(LIB) $(LIBCRYPTO) -o $@ $(LDFLAGS) 

hashcash-dll$(EXE):   $(EXEOBJS) hashcash.dll
	$(CC) $(EXEOBJS) hashcash.dll -o $@ $(LDFLAGS)

sha1test$(EXE):	sha1test.o libsha1.o
	$(CC) sha1test.o libsha1.o -o $@ $(LDFLAGS)

all:	$(EXES)

libhashcash$(LIB):	$(LIBOBJS)
	$(DELETE) $@
	$(AR) rcs $@ $(LIBOBJS)

hashcash.dll:   $(LIBOBJS)
	$(CC) -shared -o hashcash.dll $(LIBOBJS) \
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
	$(DELETE) *.bak test/* *.dll *.lib *.exe *.a *.sdb

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
fastmint_library.o: sha1.h types.h libfastmint.h hashcash.h
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
