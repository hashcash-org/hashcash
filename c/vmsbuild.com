$! VAX VMS script to build HASHCASH.EXE and SHA1.EXE
$!
$! type @vmsbuild to run
$!
$ cc hashcash.c,libsha1.c,timer.c,sdb.c,utct.c,random.c,sha1.c,libhc.c
$ link hashcash,libsha1,timer,sdb,utct,random,libhc
$ link sha1,libsha1
