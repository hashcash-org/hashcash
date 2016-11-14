#!/bin/sh

vers=1.00
sha1=/usr/bin/sha1sum		# where your sha1 lives
#sha1=/usr/bin/sha1
dbfile="hashcash.sdb"
defbits=20

mins=60; let "hours = 60 * $mins"; let "days = 24 * $hours"
let "weeks = 7 * $days"; let "years = 365 * $days"; let "months = $years / 12"

let "expiry = 28 * $days"    # default expiry 28 days
let "grace = 2 * $days"

function usage() {
    cat <<EOF
Usage:	hashcash.sh [-c][-b bits][-d][-h]
	-b bits sets required bits
	-m mint stamps
	-c check expiry
	-d check double spend db
	-e expiry
	-f dbfile
	-g grace period
	-h help
	-r resource
	-t time -- absolute or relative with +/-
	-u -- give time args in UTC
	-v -- give version number
EOF
    exit 0
}

function hc_bits() {
    hash=`echo -n $1 | $sha1`
    zeros=`echo -n $hash | sed 's/^\(0*\).*$/\1/' | wc -c`
    last=`echo -n $hash | sed 's/^0*\(.\).*$/\1/'`
    lastbits=`echo -n $last | \
	sed -e 'y/123/xyz/' -e 's/[89abcdef]/0/' -e 's/[7654]/1/' \
	-e 's/[zy]/2/' -e 's/x/3/'`
    let "bits = $zeros * 4 + $lastbits"
    echo $bits
}

function db_in() {
    if [ ! -f $dbfile ]; then 
	echo "last_purged 700101" > hashcash.sdb
    fi
    grep -q $1 hashcash.sdb && return 0 || return 1
}

function db_add() {
    echo $1 $expiry >> $dbfile
}

function hc_check() {
    tbits=`hc_bits $1`
    ver=`echo $1 | cut -d: -f 1`
    if [ "$ver" == 0 ]; then fld=1-3; cbits=0; else
        fld=1,3,4;cbits=`echo $1 | cut -d: -f 2`
    fi
    if ! hc_ok `echo $1 | cut -d: -f $fld --output-delimiter ' '` "$2" "$3"; 
    then return 1; 
    fi
    if [ "$ver" -ge 1 -a "$cbits" -lt "$tbits" ]; then tbits=$cbits; fi
    if [ "$ver" -ge 1 -a "$cbits" -gt "$tbits" ]; then tbits=0; fi
    if [ "$bits" -a "$tbits" -lt "$3" ]; then 
	echo stamp has insufficient bits 1>&2; return 1; 
    fi
    if [ "$db" != "" ]; then
  	if db_in $1; then echo stamp spent 1>&2; return 1; fi
    	db_add $1
    fi
    if [ "$check" -a "$db" -a "$bits" -a "$res" ]; then
    	echo ok
    else
	echo -n "ok, but not fully checked as "
	if [ ! "$bits" ]; then echo -n "bits"; p=1; fi
	if [ ! "$check" ]; then 
	    if [ "$p" ]; then echo -n ", "; fi;
	    echo -n "expiry"; p=1
        fi
	if [ ! "$res" ]; then 
	    if [ "$p" ]; then echo -n ", "; fi;
	    echo -n "resource"; p=1
        fi
	if [ ! "$db" ]; then 
	    if [ "$p" ]; then echo -n ", "; fi;
	    echo -n "database"; p=1
        fi
	echo " not specified"
        return 2
    fi
}

function hc_mint() {
    rand=`dd if=/dev/urandom bs=1 count=8 2> /dev/null | od -tx1 -Ad | \
	sed -e 's/^[0-9]\+//g' -e 's/ //g' | head -1`
    count=1
    stamp="1:$bits:$today:$1::$rand:$count"
    res=`hc_bits $stamp`
    while [ `hc_bits $stamp` -lt "${bits:-20}" ]; do
	let "count = $count + 1"
 	stamp="1:$bits:$today:$1::$rand:$count"
    done 
    echo $stamp
}

# %s is a gnu date extension, not sure what to do if don't have that?

function hc_time() {
    secs=`echo $1 | sed -e 's/^\(..\)\{1,5\}$/0/' \
	-e 's/^\(..\)\{5\}\(..\)$/\2/'`
    year=${1:0:2}
    if [ "$year" -gt 69 ]; then yy=19; else yy=20; fi
    conv=`echo $1 | sed -e "s/^\([^-].\)$/-d $yy\10101/" \
	-e "s/^\([^-]...\)$/-d $yy\101/" -e "s/^\([^-].....\)$/-d $yy\1/" \
	-e "s/^\([^-].....\)\(..\)$/-d $yy\1 -d \200/" \
	-e "s/^\([^-].....\)\(....\)\(..\)\?$/-d $yy\1 -d \2/" `
    minted=`date $2 $conv +%s`
    let "minted = $minted + $secs"
    echo $minted
}

function hc_ok() {
    if [ "$1" != "0" -a "$1" != "1" ]; then 
	echo unsupported version 1>&2; return 1; fi
    minted=`hc_time "$2" -u`

    if [ "$check" ]; then 
	let "latest = $minted + $expiry + $grace"
        let "earliest = $minted - $grace"
        if [ $time -gt $latest ]; then echo stamp expired 1>&2; return 1; fi
        if [ $time -lt $earliest ]; then 
	    echo stamp has futuristic creation time 1>&2; return 1;
        fi
    fi

    if [ "$4" -a $3 != "$4" ]; then echo stamp has wrong resource 1>&2; return 1; fi
    return 0
}

function time_conv() {
    unit=`echo $1 | sed -e 's/^[0-9]*\([^0-9]\)$/\1/'`
    t=`echo $1 | sed -e 's/^\(.*\)\([^0-9]\)$/\1/'`
    case $unit in
	s) ;;
	m) let "t = $t * $mins";;
	h) let "t = $t * $hours";;
	d) let "t = $t * $days";;
	w) let "t = $t * $weeks";;
	M) let "t = $t * $months";;
	Y|y) let "t = $t * $years";;
    esac
    echo $t	
}

now=`date +%s`
adjust=0
while getopts "b:cde:fg:hmr:t:uv" opt; do
    case $opt in
    b) case $OPTARG in
       +*) let "bits = $defbits $OPTARG";;
       -*) let "bits = $defbits $OPTARG";;
       [0-9]*) bits=$OPTARG;;
       default) bits=$defbits;;
       esac;;
    c) check=1;;
    d) db=1;;
    e) expiry=$OPTARG; expiry=`time_conv $expiry`;;
    f) dbfile=$OPTARG;;
    g) grace=`time_conv $OPTARG`;;
    h) usage;;
    m) mint=1;;
    r) res=$OPTARG;;
    t) case $OPTARG in
	+*) adjust=`time_conv ${OPTARG:1}`;;
	-*) adjust=`time_conv ${OPTARG:1}`; let "adjust = $adjust * -1";;
	[0-9]*) time=`hc_time $OPTARG $u`;;
       esac;;
    u) u="-u ";;
    v) echo "Version: hashcash.sh-$vers" 1>&2; exit 1;;
    esac
done

today=`date +%g%m%d`
time=${time:-$now}
let "time = $time + $adjust"

shift $(($OPTIND - 1))

if [ "$mint" ]; then
    if [ "$res" != "" ]; then 
	hc_mint $res
    else
	hc_mint $1
    fi
else
    if [ "$1" ]; then
	hc_check $1 ${res:-""} ${bits:-0}
    else
	read st
	hc_check $st ${res:-""} ${bits:-0}
    fi
fi
