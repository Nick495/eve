#!/bin/sh

# For use with the external hard drive.
for item in $( find -E /Volumes/Backup/2* -type f -regex '.*\.dump\.gz' )
#for item in \
    #$( find -E /Users/nick/Downloads/dumps/2* -type f -regex '.*\.dump\.gz' )
do
	date=$( echo $item | rev | cut -c 9-18 | rev )
	size=$( gzip -l ${item} | sed '1d' | awk '{print $2}' )

	## A platform-independent (with slight overhead) way to monitor the
	# progress of the script.
	# ( echo ${date} ; gunzip -c ${item} | pv -erta -s ${size} | sed '1d' )\
	#	| ./eve > log.txt

	# The following form has no monitoring, but is faster as a result. We
	# can use 'progress' to monitor it.
	( echo ${date}; gunzip -c ${item} | sed '1d' ) | ./test >>log.txt
done
