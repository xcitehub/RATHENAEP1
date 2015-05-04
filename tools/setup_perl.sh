#!/bin/sh
# Perl Setup
# Sets up perl environment with all required module dependencies.

export PERL_MM_USE_DEFAULT=1
perl -MCPAN -e 'my $c = "CPAN::HandleConfig"; $c->load(doit => 1, autoconfig => 1); $c->edit(prerequisites_policy => "follow"); $c->edit(build_requires_install_policy => "yes"); $c->commit'
MODULES='File::Basename Getopt::Long DBI YAML Cwd Net::Ping Scalar::Util Git::Repository'
MODULE2='YAML:XS local::lib'

if [ $# -eq 1 ]; then 
	echo "Running cpan using '$1' as configuration file";
	DIR=$TRAVIS_BUILD_DIR
	if [ -z $TRAVIS_BUILD_DIR ]; then DIR=$(pwd); fi
	export PERL5LIB="$DIR/.cpan/build"
	DIR=$(echo $DIR | sed -e "s!\/!\\\/!g") #escape
	echo $DIR
	echo $1
	perl -p -i -e "s/libdir/$DIR/g" $1
	cpan -j $1 $MODULES;
	cpan -j $1 $MODULE2;
	echo $PERL5LIB
else 
	cpan $MODULES;
	cpan $MODULE2;
fi
echo 'Setup finished';
#
