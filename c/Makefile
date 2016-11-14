HC_VERS = 0.30
INSTALL_PATH = /usr/local/bin
MAN_INSTALL_PATH = /usr/share/man/man1
DOC_INSTALL_PATH = /usr/share/doc/hashcash-$(HC_VERS)
MAKEDEPEND = makedepend
COPT = -O3
LDFLAGS = $(CARCH)
CFLAGS = $(COPT) $(CWARN) $(CARCH)
EXES = hashcash sha1 sha1test
INSTALL = /usr/bin/install
POD2MAN = pod2man
POD2HTML = pod2html
POD2TEXT = pod2text
DELETE = /bin/rm -f
ETAGS = etags
OBJS = hashcash.o libsha1.o libhc.o timer.o sdb.o utct.o random.o
DIST = ../dist.csh

build:	hashcash sha1

hashcash:	$(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

sha1:	sha1.o libsha1.o
	$(CC) $(LDFLAGS) sha1.o libsha1.o -o $@

sha1test:	sha1test.o libsha1.o timer.o
	$(CC) $(LDFLAGS) sha1test.o libsha1.o timer.o -o $@

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
	$(DELETE) *.o *~ $(EXES) *.db *.bak TAGS core* *.bak

tags:
	$(ETAGS) *.c *.h

dist:	
	$(DIST)

# DO NOT DELETE

getopt.o: getopt.h
hashcash.o: sdb.h utct.h random.h timer.h types.h hashcash.h sha1.h
libhc.o: hashcash.h utct.h types.h sha1.h random.h timer.h
libsha1.o: sha1.h types.h
random.o: random.h timer.h types.h sha1.h
sdb.o: sdb.h types.h utct.h
sha1.o: sha1.h types.h
sha1test.o: sha1.h types.h timer.h
timer.o: timer.h types.h
utct.o: utct.h
hashcash.o: utct.h types.h
sha1.o: types.h
timer.o: types.h
