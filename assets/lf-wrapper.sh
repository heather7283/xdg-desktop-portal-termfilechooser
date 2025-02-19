#!/bin/sh

set -x

die() {
    printf "$@" >&2
    exit 1
}

pipe_fd="$1"
type="$2"

case "$type" in
    (0) # SaveFile
        current_folder="$3"
        current_name="$4"

        suggested_file_path="${current_folder}/${current_name}"
        if [ -e "$suggested_file_path" ]; then
            suggested_file_path="${suggested_file_path}.$(head -c 8 /dev/urandom | base32)"
        fi
        touch "$suggested_file_path"

        foot -- \
            lf \
            -single \
            -command ':cmd confirm &fdmove -c 1 '"$pipe_fd"' echo "$fs"' \
            -command ':map <enter> :confirm; :quit' \
            "$suggested_file_path"
        ;;
    (*)
        die "Invalid type: %d\n" "$type"
esac


