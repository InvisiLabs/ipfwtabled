#!/bin/sh
#
# rcNG startup script for ipfwtabled
#
# PROVIDE: ipfwtabled
# REQUIRE: NETWORKING SERVERS
# BEFORE: DAEMON
# KEYWORD: shutdown
#
# Add the following lines to /etc/rc.conf to enable ipfwtabled:
# ipfwtabled_enable (bool):	Set it to "YES" to enable ipfwtabled.
#				Default is "NO".
# ipfwtabled_flags (flags):	Set flags to alter default behaviour of ipfwtabled.
# 				call ipfwtabled with -h for more info.
#

. /etc/rc.subr

name="ipfwtabled"
rcvar=${name}_enable

load_rc_config $name

: ${ipfwtabled_enable="NO"}
: ${ipfwtabled_flags="-d -u -b 127.0.0.1"}

command="/usr/local/sbin/${name}"

run_rc_command "$1"
