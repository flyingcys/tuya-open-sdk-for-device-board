#!/bin/bash

set -e

#set -x

param_sort()
{
	echo $@ | xargs -n 2 | sort -n -k2
}

get_file_size()
{
    echo $(ls -l $1 | awk '{print $5}')
}

usage()
{
    echo "Usage: cmd [outfile] [size] [file1 ofs1] [file2 ofs2] [file3 ofs3] ..."
    echo ""
    echo "outfile: the output file name, default output.bin"
    echo "siez: the output file size limited"
    echo "file1 ofs1: the file and its offset"
    echo ""
}

out=$1
outfile_size=$2

if [ -z $out ];then
    echo "no output file name, default output.bin"
    out=output.bin
fi

if [ -z $outfile_size ];then
    echo "No outfile_size spec, exit"
    exit 1
fi

shift 2

set -- $(param_sort $@)

files=($@)

for ((i=0;i<$(($#));i=$((i+2)))); do
    #Get current file size and its ofs in output file.
    ofs=${files[$((i+1))]}
    file_size=$(get_file_size ${files[i]})
    echo "file: ${files[i]} ofs: $ofs size: $file_size"

    #Calculate whether the total size is longger than limited size.
    total_size=$(($total_size+$file_size))

    #whether the file offset and its size is bigger than limited size
    endaddr=$(($file_size+$ofs))
    if [ $endaddr -gt $outfile_size ]; then
        echo "${files[i]} is too big or its offset is error."
        exit 2
    fi

    #Determining address Conflicts
    if [ $i -ge 2 ];then
        occupied=$(($occupied+${files[i-1]}+$(get_file_size ${files[i-2]})))
    else
        occupied=0
    fi

    if [ $ofs -lt $occupied ]; then
        echo "Address conflicts."
        echo " $ofs --- $occupied"
        exit 3
    fi
done

if [ $total_size -gt $outfile_size ];then
    echo "Error total file size is greater than output file size"
    exit 3
else
    echo $total_size $outfile_size
fi

if [ -f $out ]; then
    rm $out
fi

echo "--- start -----------------"
for ((i=0;i<$(($#));i=$((i+2)))); do

    file_size=$(get_file_size ${files[i]})
    ofs=${files[i+1]}

    if [ $i -ge 2 ]; then
        last_file_size=$(get_file_size ${files[i-2]})
        last_file_ofs=${files[i-1]}
        last_ofs=$(($last_file_ofs+$last_file_size))
    else
        last_ofs=0
    fi

    fill=$(($ofs-$last_ofs))
    if [ $fill -ne 0 ]; then
        echo "fill $out, seek $last_ofs, fill $fill bytes in $out"
        dd if=/dev/zero bs=1 count=$fill | sed -n 's/\x00/\xff/gp' | dd bs=1 count=$fill seek=$last_ofs of=$out
    fi

    #attach to output file
    echo "write ${files[i]}, seek $ofs, write $file_size"
    dd if=${files[i]} of=$out bs=1 count=$file_size seek=$ofs

    echo "--------------------"
done
echo "--- end -----------------"



