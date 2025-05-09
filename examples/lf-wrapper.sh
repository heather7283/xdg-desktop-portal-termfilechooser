#!/bin/sh

set -x

# $1 - Request type. 0 is SaveFile, 1 is SaveFiles, 2 is OpenFile
# For SaveFile:
#   $2 - Suggested folder in which the file should be saved.
#   $3 - Suggested name of the file.
# For OpenFile:
#   $2 - Suggested folder from which the files should be opened.
#   $3 - 1 if multiple files can be selected, 0 otherwise.
#   $4 - 1 if folders should be selected instead of files, 0 otherwise.
#
# Your script should write paths, each ending with newline, to fd 4.

die() {
    echo "$1" >&2
    exit 1
}

dummy_file="$(cat <<'EOF'
Saving files tutorial

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!        === WARNING! ===        !!!
!!! The contents of whatever file  !!!
!!! you select will be OVERWRITTEN !!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

Instructions:
1) Move this file wherever you want.
2) Rename the file if needed.
3) Press Enter to confirm selection.

Notes:
1) This file is provided only for your
   convenience. You can delete it and
   choose another file to overwrite.
2) If you quit without pressing Enter
   this file will be removed and save
   operation aborted.
EOF
)"

type="$1"

case "$type" in
    (0) # SaveFile
        current_folder="$2"
        current_name="$3"

        suggested_file_path="${current_folder}/${current_name}"
        # keep appending _ to suggested file path if it already exists
        while [ -e "$suggested_file_path" ]; do
            suggested_file_path="${suggested_file_path}_"
        done
        echo "$dummy_file" >"$suggested_file_path"

        # This is what will be displayed at the top of lf window, see lf docs for details
        promptfmt=' \033[1;31mSaving file:\033[0m \033[1;34m%w/\033[1;37m%f\033[0m'

        # launch lf running in foot.
        # map enter key to execute echo with selected files as arguments,
        # with its output redirected to pipe file descriptor provided by portal,
        # and then quit lf.
        foot \
            lf \
            -command "set promptfmt \"${promptfmt}\"" \
            -command 'cmd confirm $echo "$fx" >&4' \
            -command 'map <enter> :confirm; quit' \
            "$suggested_file_path"
        ;;
    (2) # OpenFile
        current_folder="$2"
        multiple="$3"
        directory="$4"

        if [ "$multiple" = "1" ]; then
            confirm_cmd='echo "$fx" >&4'
            promptfmt=' \033[1;32mOpening files:\033[0m \033[1;34m%w/\033[1;37m%f\033[0m'
        else
            confirm_cmd='echo "$f" >&4'
            promptfmt=' \033[1;32mOpening file:\033[0m \033[1;34m%w/\033[1;37m%f\033[0m'
        fi

        if [ "$directory" = "1" ]; then
            dironly_cmd='set dironly true'
            promptfmt=' \033[1;32mOpening directory:\033[0m \033[1;34m%w/\033[1;37m%f\033[0m'
        else
            dironly_cmd='set dironly false'
        fi

        foot \
            lf \
            -command "$dironly_cmd" \
            -command "set promptfmt \"${promptfmt}\"" \
            -command 'cmd confirm $'"${confirm_cmd}" \
            -command 'map <enter> :confirm; quit' \
            "$current_folder"
        ;;
    (*)
        die "Invalid request type: ${type}"
esac


