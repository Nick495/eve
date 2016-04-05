#!/bin/sh

#mkfifo ./lol.pipe

#time ./test < ./lol.pipe >log.txt &
#iprofiler -systemtrace ./test < ./lol.pipe >log.txt &

# Redirecting 3 to pipe keeps the pipe open over multiple iterations through
# the loop, as opposed to closing it each time.

# open the pipe for writing
#exec 3>./lol.pipe

# For use with the external hard drive.
#for item in $( find -E /Volumes/Backup/2* -type f -regex '.*\.dump\.gz' )
# For updating
for item in $( find -E ~/Downloads/dumps/new/ -type f -regex '.*\.dump\.gz' )
do
	date=$( echo $item | rev | cut -c 9-18 | rev )
	size=$( gzip -l ${item} | sed '1d' | awk '{print $2}' )

	# The following form has no monitoring, but is faster as a result. We
	# can use 'progress' to monitor it.
	echo "Processing - ${date}"
	#( echo ${date}; gunzip -c ${item} ) >&3
	( echo ${date}; gunzip -c ${item} ) | ./test >> ./log.txt

done

# Close pipe.
exec 3>&-
