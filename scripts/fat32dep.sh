#!/bin/bash

FILE=build/"fat32.dep"

if [ ! -f "$FILE" ]; then
    touch $FILE
    echo $FILE
fi