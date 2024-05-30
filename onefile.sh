#!/bin/bash

script_path=$(cd "$(dirname "$0")" && pwd)

filelist=(
    "$script_path/abuf/ab_global.h" 
    "$script_path/abuf/ab_error.h"
    "$script_path/abuf/ab_os.h"
    "$script_path/abuf/ab_index.h"
    "$script_path/abuf/ab_mem.h"
    "$script_path/abuf/ab_list.h"
    "$script_path/abuf/ab_hash.h"
    "$script_path/abuf/ab_skiplist.h"
    "$script_path/abuf/ab_json.h"
    "$script_path/abuf/ab_buf.h"
    "$script_path/abuf/ab_error.c"
    "$script_path/abuf/ab_os.c"
    "$script_path/abuf/ab_mem.c"
    "$script_path/abuf/ab_list.c"
    "$script_path/abuf/ab_hash.c"
    "$script_path/abuf/ab_skiplist.c"
    "$script_path/abuf/ab_jsonbin.c"
    "$script_path/abuf/ab_jsonpath.c"
    "$script_path/abuf/ab_buf.c"
)

cfile=$script_path/anybuf.c
> ${cfile}
for file in ${filelist[@]};do
    if [ "${file##*.}" = "h" ]; then
        sed "s/__AB_.*__/__onefile_$(basename ${file%.*})__/g" ${file} |sed "s/#include.*\"ab_.*.h\"//g" >> ${cfile}
    else
        cat ${file}|sed "s/#include.*\"ab_.*.h\"//g" >> ${cfile}
    fi

    echo >> ${cfile}
done

hfile=$script_path/anybuf.h
> ${hfile}
cat $script_path/abuf/ab_buf.h  >> ${hfile}
cat $script_path/abuf/ab_json.h >> ${hfile}

