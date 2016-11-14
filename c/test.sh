#!/bin/sh

hashcash="../hashcash -u -t 040404"
sha1="../sha1"
mkdir -p test
cd test

#v=1
test=1

######################################################################

echo -n "test $test (-mb8) "
$hashcash -mqb8 foo@bar.com > stamp.$test
echo -n `cat stamp.$test` | $sha1 | sed 's/^\(..\).*/\1/' > res.$test
echo 00 > out.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-mb12) "
$hashcash -mqb12 foo@bar.com > stamp.$test
echo -n `cat stamp.$test` | $sha1 | sed 's/^\(...\).*/\1/' > res.$test
echo 000 > out.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-mb8 -mb12) "
$hashcash -mqb8 foo@bar.com -b12 taz@bar.com > stamp.$test
echo -n `head -1 stamp.$test` | $sha1 | sed 's/^\(..\).*/\1/' > res1.$test
echo 00 > out1.$test
echo -n `tail -1 stamp.$test` | $sha1 | sed 's/^\(...\).*/\1/' > res2.$test
echo 000 > out2.$test
diff -q res1.$test out1.$test 1> /dev/null 2>&1 && \
diff -q res2.$test out2.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-b10 partial) "
$hashcash -q -b10 < stamps
[ $? -eq 2 ] && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-b15 insufficient) "
$hashcash -q -b15 < stamps
[ $? -eq 1 ] && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-r partial) "
$hashcash -q -r '*@foo.com' < stamps
[ $? -eq 2 ] && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-r no match) "
$hashcash -q -r '*@taz.com' < stamps
[ $? -eq 1 ] && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-d not spent) "
cat > db.$test <<EOF
last_purged 700101000000
0:040402:adam+bar@foo.com:0ace5ad5254b4e401036b5f0 2419200
EOF
cat > stamp.$test <<EOF
0:040402:jack+bar@foo.com:be45eb4e586a3e08cf7c95c4
EOF
$hashcash -qd -f db.$test < stamp.$test
[ $? -eq 2 ] && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-d spent) "
cat > db.$test <<EOF
last_purged 700101000000
0:040402:adam+bar@foo.com:0ace5ad5254b4e401036b5f0 2419200
EOF
cat > stamp.$test <<EOF
0:040402:adam+bar@foo.com:0ace5ad5254b4e401036b5f0
EOF
$hashcash -qd -f db.$test < stamp.$test
[ $? -eq 1 ] && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-w) "
cat > stamps <<EOF
0:050403:space@foo.com:1f0986777f16f66a53241ec3
0:030404:fax@foo.com:23d5ea11c43287d58ea6a63f
0:040402:adam+bar@foo.com:0ace5ad5254b4e401036b5f0
0:040402:jack+bar@foo.com:be45eb4e586a3e08cf7c95c4
0:040404:fred+xyz@foo.com:20056ff4e877027ef8ba55eb
EOF

cat > out.$test <<EOF
5
5
5
12
6
EOF
#if [ $v -ne "" ]; then echo; endif
[ $v ] && echo && echo "$hashcash -wq < stamps > res.$test"
$hashcash -wq < stamps > res.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-wb10 one sufficient) "
cat > stamp.$test <<EOF
0:040402:jack+bar@foo.com:be45eb4e586a3e08cf7c95c4
EOF

cat > out.$test <<EOF
12
EOF
#if [ $v -ne "" ]; then echo; endif
[ $v ] && echo && echo "$hashcash -wq < stamp.$test > res.$test"
$hashcash -wqb10 < stamp.$test > res.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-wn) "
cat > out.$test <<EOF
space@foo.com 5
fax@foo.com 5
adam+bar@foo.com 5
jack+bar@foo.com 12
fred+xyz@foo.com 6
EOF

$hashcash -wnq < stamps > res.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-n) "
cat > out.$test <<EOF
space@foo.com
fax@foo.com
adam+bar@foo.com
jack+bar@foo.com
fred+xyz@foo.com
EOF

$hashcash -nq < stamps > res.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-nb10) "
cat > out.$test <<EOF
jack+bar@foo.com
EOF

$hashcash -nqb10 < stamps > res.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-nr three match) "
cat > out.$test <<EOF
adam+bar@foo.com
jack+bar@foo.com
EOF

$hashcash -nq -r '*+bar@foo.com' < stamps > res.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-l) "
cat > out.$test <<EOF
+31276800
-29030400
2419200
2419200
2592000
EOF

$hashcash -lq < stamps > res.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-nl) "
cat > out.$test <<EOF
space@foo.com +31276800
fax@foo.com -29030400
adam+bar@foo.com 2419200
jack+bar@foo.com 2419200
fred+xyz@foo.com 2592000
EOF

$hashcash -nlq < stamps > res.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-wl) "
cat > out.$test <<EOF
5 +31276800
5 -29030400
5 2419200
12 2419200
6 2592000
EOF

$hashcash -wlq < stamps > res.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-nwl) "
cat > out.$test <<EOF
space@foo.com 5 +31276800
fax@foo.com 5 -29030400
adam+bar@foo.com 5 2419200
jack+bar@foo.com 12 2419200
fred+xyz@foo.com 6 2592000
EOF

$hashcash -nwlq < stamps > res.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################
# -c with -y
######################################################################

echo -n "test $test (-cy -b10 match) "
$hashcash -cqy -b10 -r '*@foo.com' < stamps && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-cy -b15 insufficient) "
$hashcash -cqy -b15 -r '*@foo.com' < stamps && echo fail || echo ok
test=`expr $test + 1`

######################################################################

echo -n "test $test (-cy insufficient) "
$hashcash -cqy -b+0 -r '*@foo.com' < stamps && echo fail || echo ok
test=`expr $test + 1`

######################################################################

echo -n "test $test (-cy -b10 no match) "
$hashcash -cqy -b10 -r '*@taz.com' < stamps && echo fail || echo ok
test=`expr $test + 1`

######################################################################

echo -n "test $test (-cy -b5 expired) "
$hashcash -cqy -b5 -r 'fax@foo.com' < stamps && echo fail || echo ok
test=`expr $test + 1`

######################################################################

echo -n "test $test (-cy -b5 future) "
$hashcash -cqy -b5 -r 'space@foo.com' < stamps && echo fail || echo ok
test=`expr $test + 1`


######################################################################
# -c with exit codes
######################################################################

echo -n "test $test (-c -b10 partial) "
$hashcash -cq -b10 -r '*@foo.com' < stamps
[ $? -eq 2 ] && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-cy -b10 success) "
$hashcash -cyq -b10 -r '*@foo.com' < stamps
[ $? -eq 0 ] && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-c -b15 insufficient) "
$hashcash -cq -b15 -r '*@foo.com' < stamps
[ $? -eq 1 ] && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-c -b10 no match) "
$hashcash -cq -b10 -r '*@taz.com' < stamps
[ $? -eq 1 ] && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-c -b5 expired) "
$hashcash -cq -b5 -r 'fax@foo.com' < stamps
[ $? -eq 1 ] && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-c -b5 future) "
$hashcash -cq -b5 -r 'space@foo.com' < stamps
[ $? -eq 1 ] && echo ok || echo fail
test=`expr $test + 1`


######################################################################
# -cn
######################################################################

cat > out.$test <<EOF
jack+bar@foo.com
EOF

echo -n "test $test (-cn -b10 one sufficient) "
$hashcash -cnq -b10 -r '*@foo.com' < stamps > res.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################

cat > out.$test <<EOF
adam+bar@foo.com
jack+bar@foo.com
fred+xyz@foo.com
EOF

echo -n "test $test (-cn -b5 three valid) "
$hashcash -cnq -b5 -r '*@foo.com' < stamps > res.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################

cat > out.$test <<EOF
adam+bar@foo.com
jack+bar@foo.com
EOF

echo -n "test $test (-cn -b5 two match) "
$hashcash -cnq -b5 -r '*+bar@foo.com' < stamps > res.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "" > out.$test

echo -n "test $test (-cn none sufficient) "
$hashcash -cnqb+0 -r '*+bar@foo.com' < stamps > res.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################

cat > out.$test <<EOF 
jack+bar@foo.com
EOF

echo -n "test $test (-cn -r -o -r one sufficient) "
$hashcash -cnqb10 -r '*+bar@foo.com' < stamps > res.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`


######################################################################
# -cw
######################################################################

cat > out.$test <<EOF
5
12
6
EOF

echo -n "test $test (-cwb5 three valid) "
$hashcash -cwqb5 < stamps > res.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################

cat > out.$test <<EOF
5
12
EOF

echo -n "test $test (-cwb5 -r two valid match) "
$hashcash -cwqb5 -r '*+bar@foo.com' < stamps > res.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################
# -cl
######################################################################

cat > out.$test <<EOF
2419200
2419200
2592000
EOF

echo -n "test $test (-cl -b5 three sufficient) "
$hashcash -clq -b5 < stamps > res.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-c -b10 -r adam+bar@foo.com -o -b5 -r *@foo.com partial) "
$hashcash -cqb10 -r 'adam+bar@foo.com' -o -b5 -r '*+bar@foo.com' < stamps
[ $? -eq 2 ] && echo ok || echo fail
test=`expr $test + 1`

######################################################################

echo -n "test $test (-c -b10 -r blah -o -b6 -r blah -r *+xyz@foo.com partial) "
$hashcash -cqb10 -r 'adam+bar@foo.com' -o -b6 -r '*+bar@foo.com' \
-r '*+xyz@foo.com' < stamps
[ $? -eq 2 ] && echo ok || echo fail
test=`expr $test + 1`

######################################################################

cat > out.$test <<EOF
2419200
2419200
2592000
EOF

echo -n "test $test (-cl -b5 three sufficient) "
$hashcash -clq -b5 < stamps > res.$test
diff -q res.$test out.$test 1> /dev/null 2>&1 && echo ok || echo fail
test=`expr $test + 1`

