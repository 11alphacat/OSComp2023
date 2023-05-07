# tbl
in="$1"
# syscall_num.h
out1="$2"
# syscall_func.h
out2="$3"
# syscall_def.h
out3="$4"


grep '^[0-9]' "$in" | sort -n | (
    while read nr name entry; do
	if [ -n "$entry" ]; then
	    echo "#define SYS_$name $nr"
	fi
    done
) > "$out1"

grep '^[0-9]' "$in" | sort -n | (
    while read nr name entry; do
	if [ -n "$entry" ]; then
	    echo "[SYS_$name] $entry,"
	fi
    done
) > "$out2"

# grep '^[0-9]' "$in" | sort -n | (
#     while read nr name entry; do
#         if [ -n "$entry" ]; then
#             if [[ "$entry" == sys* ]]; then
#                 echo "[SYS_$name] $entry,"
#             else ;
#                 echo "[SYS_$name] $entry,"
#             fi
#         fi
#     done
# ) > "$out2"


grep '^[0-9]' "$in" | sort -n | (
    while read nr name entry; do
	if [ -n "$entry" ]; then
		echo "extern uint64 $entry(void);"
	fi
    done
) > "$out3"


# used ONLY ONCE to create syscallnew.c
# in2="sys2.txt"
# out4="src/kernel/syscallnew.c"
# rm $out4
# echo "#include \"debug.h\"\n#include \"common.h\"\n\n" > $out4
# cat  "$in2" | (
#     while read nr entry; do
# 	if [ -n "$entry" ]; then
# 		echo "uint64 $entry(void) {\n\tASSERT(0);\n\treturn 0;\n}\n"
		
# 	fi
#     done
# ) >> "$out4"

# uint64 sys_munmap(void) {
#     ASSERT(0);
#     return 0;
# }