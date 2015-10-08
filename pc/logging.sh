#!/bin/bash

usage(){
	echo "Usage: $0 filename"
	exit -1
}


if test $# -ne 1; then
  usage
fi


tail -n +0 -f ./flight_log.txt
