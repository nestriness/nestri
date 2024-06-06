#!/bin/bash
#TODO: Fix the warp-input startup problem
if [ -z "$SESSION_ID" ]; then
    echo "Error: SESSION_ID environment variable is not set."
    exit 1
fi

#Open udp port to listed for QUIC events on 
export PORT=${PORT:-"8080"}

sudo -E /usr/bin/supervisord -c /etc/supervisord.conf
