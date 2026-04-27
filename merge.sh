#!/bin/bash

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <output_archive> <input_library> [<additional_objects>...]"
    exit 1
fi

output_archive="$1"
input_library="$2"
shift 2

cp "$input_library" "$output_archive"

for obj in "$@"; do
    ar rcs "$output_archive" "$obj"
done

ranlib "$output_archive"
