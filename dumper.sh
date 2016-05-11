#!/bin/sh

#mkfifo ./lol.pipe

# Performance testing pipes.
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

	echo "Processing - ${date}"
	# Pipe mode stuff (Currently broken).
	#( echo ${date}; gunzip -c ${item} ) >&3
	# Regular mode (faster).
	( echo ${date}; gunzip -c ${item} ) | ./test >> ./log.txt
	# Testing.
	#( echo ${date}; gunzip -c ${item} ) | valgrind ./test

done

# Close pipe.
exec 3>&-
