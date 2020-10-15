# test client ssl certificate verification

use strict;
use warnings;

our %args = (
    client => {
	ssl => 1,
	offertlscert => 0,
	up => 'failed',
	down => 'connect attempt failed',
	dryrun => 1,
	nocheck => 1,
    },
    relayd => {
	listenssl => 1,
	verifyclient => 1,
    },
    server => {
	noserver => 1,
	nocheck => 1,
    },
);

1;
