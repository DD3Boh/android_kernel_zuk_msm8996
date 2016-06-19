#!/bin/sh

sdate=${1}
cdate=`date +"%m_%d_%Y"`
rdir=`pwd`

rm -rf changelog_*

clear

# Bash Color
green='\033[01;32m'
red='\033[01;31m'
blink_red='\033[05;31m'
restore='\033[0m'

echo -e "${red}"
echo "		----------	"
echo "		/		"
echo "		\		"
echo "		/--------	"
echo "		\		"
echo "		/		"
echo "		\		"
echo ''
echo " Welcome to Fire Kernel changelog script! "
echo -e "${restore}"

# Check the date start range is set
if [ -z "$sdate" ]; then
    echo ""
    echo "Failed!"
    echo "Add a date in mm/dd/yyyy format to count from..."
    echo ""
fi

# Find the directories to log
echo "Starting date picking based on the input date..."
find $rdir -name .git | sed 's/\/.git//g' | sed 'N;$!P;$!D;$d' | while read line
do
cd $line
    # Test to see if the repo needs to have a changelog written
    log=$(git log --pretty="%an - %s" --no-merges --since=$sdate --date-order)
    if [ -z "$log" ]; then
    echo "Nothing updated on Lambda Kernel changelog, skipping..."
    else
        # Write the changelog
        echo "Changelog is updated and written for $project..."
        echo "Project: $project" >> "$rdir"/changelog_$cdate.log
        echo "$log" | while read line
        do
echo "$line" >> "$rdir"/changelog_$cdate.log
        done
echo "" >> "$rdir"/changelog_$cdate.log
    fi
done
echo -e "${red}"
echo "Changelog script for Fire Kernel has finished."
echo -e "${restore}"
