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
sudo sed -i "s|^command.*=.*$|command=bash -c \"$escaped_xarg\"|" /etc/supervisord.d/game.ini

sudo sed -i 's|^autostart.*=.*$|autostart=true|' /etc/supervisord.d/game.ini

sudo -E /usr/bin/supervisord -c /etc/supervisord.conf
