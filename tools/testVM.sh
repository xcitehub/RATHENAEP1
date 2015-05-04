#!/bin/sh
#by lighta

for i in $@
do
 cd ..
 case $i in
	'raCent65')
	'raFed21')
	'raUbun64')
	'raWin12')
	'raOsX10')
		vagrant up $i
		vagrant provision $i 
		sleep 300
		vagrant halt $i
	;;
	*) 
		echo "$i is not supported yet, or yu made a typo"
	;;
 esac
done
