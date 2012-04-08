#!/usr/bin/env perl

use strict;
use IO::Socket::INET;
use IO::Socket::UNIX;

use constant CMD_ADD    => 1;
use constant CMD_DEL    => 2;
use constant CMD_FLUSH  => 3;

my $usage = <<EOF
client.pl <host> <port> <table> <command> <ip[/mask]>
EOF

my ($host, $port) = split(/:/, $addr);
$port = 56789 unless $port;
unless ($host && $port && $table && $cmd =~ /^(add|del|flush)$/) {
        print STDERR $usage;
        exit 1;
}

