#!/bin/bash

if [ -z "$SESSION_ID" ]; then
    echo "Error: SESSION_ID environment variable is not set."
    exit 1
fi

xarg="$*"

if [ -z "$xarg" ]; then
    echo "Error: No command specified to run the game. Exiting."
    exit 1
fi

escaped_xarg=$(printf '%s\n' "$xarg" | sed -e 's/[\/&]/\\&/g')

sed -i "s|^command.*=.*$|command=bash -c \"$escaped_xarg\"|" /etc/supervisor.d/game.ini

sed -i 's|^autostart.*=.*$|autostart=true|' /etc/supervisor.d/game.ini

/usr/bin/supervisord -c /etc/supervisord.conf --nodaemon --user root
