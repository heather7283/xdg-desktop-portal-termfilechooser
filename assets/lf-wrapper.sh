#!/bin/sh

# $1 - File descriptor. Your script should write paths, each ending with newline, to this fd.
# $2 - Request type. 0 is SaveFile, 1 is SaveFiles, 2 is OpenFile
# For SaveFile:
#   $3 - Suggested folder in which the file should be saved.
#   $4 - Suggested name of the file.
# For OpenFile:
#   $3 - Suggested folder from which the files should be opened.
#   $4 - 1 if multiple files can be selected, 0 otherwise.
#   $5 - 1 if folders should be selected instead of files, 0 otherwise.

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

        foot \
            lf \
            -command 'cmd confirm $fdmove -c 1 '"${pipe_fd}"' echo "$fx"' \
            -command 'map <enter> :confirm; quit' \
            "$suggested_file_path"
        ;;
    (2) # OpenFile
        current_folder="$3"
        multiple="$4"
        directory="$5"

        if [ "$directory" = "1" ]; then
            dironly_cmd='set dironly true'
        else
            dironly_cmd='set dironly false'
        fi

        if [ "$multiple" = "1" ]; then
            confirm_cmd='fdmove -c 1 '"${pipe_fd}"' echo "$fx"'
        else
            confirm_cmd='fdmove -c 1 '"${pipe_fd}"' echo "$f"'
        fi

        foot \
            lf \
            -command "$dironly_cmd" \
            -command 'cmd confirm $'"${confirm_cmd}" \
            -command 'map <enter> :confirm; quit' \
            "$current_folder"
        ;;
    (*)
        die "Invalid request type: %d\n" "$type"
esac


