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
	    echo "#define SYS_$name      \t$nr"
	fi
    done
) > "$out1"
grep '^[0-9]' "$in" | sort -n | (
    while read nr name entry; do
	if [ -n "$entry" ]; then
	    echo "[SYS_$name]      \t$entry,"
	fi
    done
) > "$out2"
grep '^[0-9]' "$in" | sort -n | (
    while read nr name entry; do
	if [ -n "$entry" ]; then
		echo "extern uint64 sys_$name(void);"
	fi
    done
) > "$out3"
