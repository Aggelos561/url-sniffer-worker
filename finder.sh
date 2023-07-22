#!/bin/bash

 
for var in "$@"
do
    count=0
    tld=".$var "
    for file in ../results/* 
    do  
        while read -r line;
        do

            words=($line)
            url="${words[0]} "
            
            if [[ "$url" == *"$tld"* ]]
            then
                count=$((count+${words[1]}))
            fi

        done < $file
    done
    echo "Number of appearances for $var is $count"
done


