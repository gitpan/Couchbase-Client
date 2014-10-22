package CouchDebug;
use strict;
use warnings;
use blib;
use base qw(Couchbase::Client);
use Data::Dumper;

use Log::Fu { level => "debug" };
use Getopt::Long;
GetOptions(
    'loop|l=i' => \my $Loops,
    'sleep|s=i' => \my $Sleep);

sub v_debug {
    my ($self,$key) = @_;
    my $ret = $self->get($key);
    my $value = $ret->value;
    if(defined $value) {
        log_infof("Got %s=%s OK", $key, $value);
    } else {
        log_errf("Got error for %s: %s (%d)", $key,
                 $ret->errstr, $ret->errnum);
        my $errors = $self->get_errors;
        foreach my $errinfo (@$errors) {
            my ($errnum,$errstr) = @$errinfo;
            log_errf("%s (%d)", $errstr,$errnum);
        }
    }
}

sub k_debug {
    my ($self,$key,$value) = @_;
    #log_debug("k=$key,v=$value");
    my $status = $self->set($key, $value);
    if($status->is_ok) {
        log_infof("Setting %s=%s OK (errnum=%d)", $key, $value, $status->errnum);
    } else {
        my $errors = $self->get_errors;
        foreach my $errinfo (@$errors) {
            my ($errnum,$errstr) = @$errinfo;
            log_errf("%s (%d)", $errstr,$errnum);
        }

        log_errf("Setting %s=%s ERR: %s (%d)",
                 $key, $value,
                 $status->errstr, $status->errnum);
    }
}

sub runloop {
    my $o = shift;
    my @klist = qw(Foo Bar Baz Blargh Bleh Meh Grr Gah);
    $o->k_debug($_, $_."Value") for @klist;
    $o->v_debug($_) for @klist;
    $o->v_debug("NonExistent");
    $o->set("foo", "bar", 100);
    $o->append("foo", "more_bar");
    my $v = $o->get("foo")->value();
    log_info("Append: ", $v);
    
    $o->add("not_here_yet", "some_value");
    
    $o->prepend("not_here_yet", "are we here?: ");
    
    log_infof("add: %s",
              $o->get("not_here_yet")->value);
    
    log_infof("add (error): %s",
              $o->add("not_here_yet", "This won't show")
              ->errstr);
    
    log_infof("replace (OK is 0): %d, %s",
              $o->replace("not_here_yet", "something_else")
              ->errnum,
              $o->get("not_here_yet")->value);
    
    log_infof("replace (err): %s",
              $o->replace("NonExistent", "something")->errstr);
    
    log_infof("Old counter is %d", $o->get("Counter")->value);
    log_infof("New counter (+42) is %d",
              $o->arithmetic("Counter", 42, 0)->value);
    log_infof("Counter (-5) is %d",
              $o->decr("Counter", 5)->value);
    
    $o->set("delete_me_soon", "meh");
    log_infof("Remove: %d", $o->delete("delete_me_soon")->errnum);
    log_infof("Remove (err) %d", $o->delete("NonExistent")->errnum);
    
    log_infof("Complex (Serialized) = %d",
              $o->set(complex_var => [qw(foo bar baz)])->errnum);
    
    log_infof("Complex (Deserialized) = %s",
              Dumper($o->get("complex_var")->value));
    
    log_infof("Compression: %d",
              $o->set("compressed_key", 'x' x 1000)->errnum);
    
    log_infof("Decompression (length=%d)",
              length($o->get("compressed_key")->value));

}

if(!caller) {
    my $o = __PACKAGE__->new({
        server => '10.0.0.99:8091',
        username => 'Administrator',
        password => '123456',
        #bucket  => 'nonexist',
        bucket => 'membase0',
        compress_threshold => 100,
    });
    bless $o, __PACKAGE__;

    $Sleep = 0 unless defined $Sleep;

    if($Loops) {
        if($Sleep < 1) {
            $Log::Fu::SHUSH = 1;
        } elsif ($Sleep) {
            Log::Fu::set_log_level(__PACKAGE__, "warn");
        }
        if(!$Sleep) {
            $o->runloop() for (0..$Loops);
        } else {
            foreach (0..$Loops) {
                $o->runloop();
                sleep($Sleep);
            }
        }
    } else {
        $o->runloop();
    }
    #my $stats = $o->stats([""]);
    #print Dumper($stats);    
}
