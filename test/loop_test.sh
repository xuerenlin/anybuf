#!/bin/bash

while [ 1 ];
do 
	./test_ab_skiplist > out.txt

	grep "OK (5 tests)" out.txt
	if [ $? -eq 0 ]
	then
		echo "$(date) test ok"
	else
		echo "test fail"
		break
	fi
	
	sleep 0.5
done

