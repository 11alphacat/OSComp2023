# tbl
in="$1"
# syscall_num.h
out1="$2"
# syscall_func.h
out2="$3"
grep '^[0-9]' "$in" | sort -n | (
    while read nr name entry; do
	if [ -n "$entry" ]; then
	    echo "#define SYS_$name    $nr"
	fi
    done
) > "$out1"
grep '^[0-9]' "$in" | sort -n | (
    while read nr name entry; do
	if [ -n "$entry" ]; then
	    echo "[SYS_$name]    $entry,"
	fi
    done
) > "$out2"