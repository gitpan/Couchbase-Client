#!/usr/bin/perl
use Dir::Self;
use lib __DIR__ . "../lib";
use lib __DIR__ . "../";

$Log::Fu::LINE_PREFIX = '#';

my $config = do 'PLCB_Config.pm';
use Couchbase::Test::Common;
my $TEST_PORT;

my $jarurl = $config->{COUCHBASE_MOCK_JARURL};
my $jarfile = __DIR__ . "/tmp/CouchbaseMock.jar";
if (! -e $jarfile) {
    warn("Can't find JAR. Downloading.. $jarurl");
    system("wget -O $jarfile $jarurl");
}

Couchbase::Test::Common->Initialize(
    jarfile => $jarfile,
    nodes => 5,
    buckets => [{name => "default", type => "memcache"}],
);

use Couchbase::Test::ClientSync;
use Couchbase::Test::Async;
use Couchbase::Test::Settings;
use Couchbase::Test::Interop;
use Couchbase::Test::Netfail;
use Couchbase::Test::Views;

Couchbase::Test::ClientSync->runtests();
Couchbase::Test::Async->runtests();
Couchbase::Test::Settings->runtests();
Couchbase::Test::Interop->runtests();
Couchbase::Test::Netfail->runtests();
Couchbase::Test::Views->runtests();
#Test::Class->runtests();
