HC_VERS = 0.14
INSTALL_PATH = /usr/local/bin
MAN_INSTALL_PATH = /usr/local/man/man1
MAKEDEPEND = makedepend
#CWARN = -Wall -pedantic # gcc warnings
#CARCH = -64   # MIPS 64 bit integers
#CWARN = -pedantic
#COPT = -g # unix debug
COPT = -O
LDFLAGS = $(CARCH)
CFLAGS = $(COPT) $(CWARN) $(CARCH)
EXES = hashcash sha1 sha1test
INSTALL = /usr/bin/install
POD2MAN = pod2man
POD2HTML = pod2html
POD2TEXT = pod2text
DELETE = /bin/rm -f
OBJS = hashcash.o libsha1.o timer.o sdb.o utct.o random.o
TAR = tar
COMPRESS = gzip

hashcash:	$(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

sha1:	sha1.o libsha1.o
	$(CC) $(LDFLAGS) sha1.o libsha1.o -o $@

sha1test:	sha1test.o libsha1.o timer.o
	$(CC) $(LDFLAGS) sha1test.o libsha1.o timer.o -o $@

all:	$(EXES)

docs:	hashcash.1 hashcash.html hashcash.txt 

hashcash.1:	hashcash.pod
	$(POD2MAN) -s 1 -c hashcash -r $(HC_VERS) $? > $@

hashcash.html:	hashcash.pod
	$(POD2HTML) --title hashcash $? > $@
	$(DELETE) pod2htm*

hashcash.txt: hashcash.pod
	$(POD2TEXT) $? > $@

install:	hashcash sha1 hashcash.1
	$(INSTALL) hashcash sha1 $(INSTALL_PATH)
	$(INSTALL) hashcash.1 $(MAN_INSTALL_PATH)

depend:
	$(MAKEDEPEND) -- -Y *.c *.h

docclean:
	$(DELETE) hashcash.txt hashcash.1 hashcash.html pod2htm*

clean:
	$(DELETE) *.o *~

distclean:
	$(DELETE) *.o *~ $(EXES) *.db *.bak

# DO NOT DELETE

hashcash.o: sdb.h utct.h random.h timer.h types.h hashcash.h
libsha1.o: sha1.h types.h
random.o: random.h timer.h types.h
sdb.o: sdb.h types.h utct.h
sha1.o: sha1.h types.h
sha1test.o: sha1.h types.h timer.h
timer.o: timer.h types.h
utct.o: utct.h
sha1.o: types.h
timer.o: types.h
