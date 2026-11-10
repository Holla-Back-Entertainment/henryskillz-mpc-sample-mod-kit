#!/bin/sh
cd "$(dirname "$0")" || exit 1
python3 tools/easy_builder.py
status=$?
printf '\nPress Return to close...'
read answer
exit "$status"
