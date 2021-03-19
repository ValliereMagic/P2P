#!/bin/bash

# If there are less than 2 arguments, show the help screen.
if [ $# -lt 2 ]; then
    echo "Expected a folder and a filename as arguments"
	echo "merge.sh <folder_path_to_work_within> <resultant_filename> "
    exit 1
fi

# Expect a filename and folder as parameters
folder=$1
filename=$2

# Change directories to the folder
cd $folder

# Get the file names for the list of chunks
chunks="$(ls -a | grep .p2p | sort -g)"

# Empty the file if it exists
truncate $filename --size 0

# Merge the chunks together
for chunk in $chunks
do
	echo "Merging ${chunk} into ${filename}"
	cat $chunk >> $filename
done
exit 0
