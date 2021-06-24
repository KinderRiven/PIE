###
 # @Date: 2021-04-17 12:20:13
 # @LastEditors: Han Shukai
 # @LastEditTime: 2021-04-17 14:05:53
 # @FilePath: /SplitKV/benchmark/go-ycsb/generator.sh
###

YCSB_BIN=/home/hanshukai/import_libs/go-ycsb/bin/go-ycsb
IN_PATH=script_test
OUT_PATH=workload
NAME=('workloada' 'workloadb' 'workloadc' 'workloadd' 'workloade' 'workloadf')
num_rw=${#NAME[@]}

mkdir -p $OUT_PATH

for ((i=0; i<${num_rw}; i+=1))
do
    $YCSB_BIN load basic -P $IN_PATH/${NAME[$i]} > $OUT_PATH/${NAME[$i]}.load
    $YCSB_BIN run  basic -P $IN_PATH/${NAME[$i]} > $OUT_PATH/${NAME[$i]}.run
done
