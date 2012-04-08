#!/usr/bin/env perl

#  Copyright (c) 2012,
#  Vadym S. Khondar <v.khondar at invisilabs.com>, InvisiLabs.
#  All rights reserved.
#  
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#      * Redistributions of source code must retain the above copyright
#        notice, this list of conditions and the following disclaimer.
#      * Redistributions in binary form must reproduce the above copyright
#        notice, this list of conditions and the following disclaimer in the
#        documentation and/or other materials provided with the distribution.
#      * Neither the name of the InvisiLabs nor the
#        names of its contributors may be used to endorse or promote products
#        derived from this software without specific prior written permission.
#  
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS BE LIABLE FOR ANY
#  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
#  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
#  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
#  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
#  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Exaple Perl client for IPFWTABLED daemon.

use strict;
use IO::Socket::INET;
use IO::Socket::UNIX;

use constant VERSION    => 1;
use constant CMD_ADD    => 1;
use constant CMD_DEL    => 2;
use constant CMD_FLUSH  => 3;

my $usage = <<EOF;
client.pl <type> <addr> <table> <command> [<subject>]
  <type> = { 'stream' | 'dgram' }
  <addr> = { /path/to/domain.sock | {<hostname>|<ipaddr>}[:<port>] }
  <table> = { 0..IPFW_TABLES_MAX }
  <command> = { 'add' | 'del' | 'flush' }
  <subject> = { <ip>[:<port>] }
EOF

my ($type, $addr, $table, $cmd, $subject) = @ARGV;

my ($host, $port) = split(/:/, $addr);
$port = 12345 unless $port;
unless (defined($host) && defined($port) && defined($table) &&
        $cmd =~ /^(add|del|flush)$/ &&
        $type =~ /^(stream|dgram)$/) {

        print STDERR $usage;
        print "LOL\n";
        exit 1;
}
my @ip = (0, 0, 0, 0);
my $mask = 32;
if ($subject) {
  if ($subject !~ /^(\d+)\.(\d+)\.(\d+)\.(\d+)(?:\/)?(\d+)?$/) {
    print STDERR $usage;
        print "LOL2\n";
    exit 1;
  }
  @ip = ($1, $2, $3, $4);
  $mask = $5 if $5;
}

my $sock;

if ($host =~ /^\//) {

  if ($type =~ /stream/) {
    $type = SOCK_STREAM;
  } else {
    $type = SOCK_DGRAM;
  }

  $sock = IO::Socket::UNIX->new(
    Peer => $host,
    Type => $type) or die "Can't create socket: $@\n";
} else {

  if ($type =~ /stream/) {
    $type = 'tcp';
  } else {
    $type = 'udp';
  }

  $sock = IO::Socket::INET->new(
    Proto    => $type,
    PeerPort => $port,
    PeerAddr => $host) or die "Can't create socket: $@\n";
}

my $msg = pack("C", VERSION);
$msg .= pack("C", $table);
if ($cmd eq 'add') {
  $msg .= pack("C", CMD_ADD);
} elsif ($cmd eq 'del') {
  $msg .= pack("C", CMD_DEL);
} elsif ($cmd eq 'flush') {
  $msg .= pack("C", CMD_FLUSH);
}
$msg .= pack("C", $mask);
$msg .= pack("C4", @ip);

$sock->send($msg) or die "send: $!";

$sock->close();
