#!/bin/bash
# Grab one still frame from the greenhouse camera (Aqara G100) RTSP feed.
# Runs from cron on tiger-pi. Deployed to ~/bin/greenhouse-snapshot.
#
# The RTSP URL (with username/password) lives in ~/.config/greenhouse-cam/rtsp_url
# (one line, chmod 600): the 1080p URL shown in the Aqara app's RTSP LAN preview settings.
# Snapshots go to ~/greenhouse-snapshots/YYYY-MM-DD/HHMM.jpg; kept for a year.

set -uo pipefail

CONF="$HOME/.config/greenhouse-cam/rtsp_url"
OUT="$HOME/greenhouse-snapshots"
LOG="$OUT/snapshot.log"
KEEP_DAYS=365

mkdir -p "$OUT"
log() { echo "$(date '+%F %T') $*" >> "$LOG"; }

if [ ! -r "$CONF" ]; then
	log "ERROR: $CONF missing"
	exit 1
fi
URL=$(head -n1 "$CONF" | tr -d '[:space:]')

day=$(date +%F)
file="$OUT/$day/$(date +%H%M).jpg"
mkdir -p "$OUT/$day"

grab() {
	# Strip credentials from any ffmpeg error text before logging it.
	timeout 60 ffmpeg -nostdin -hide_banner -loglevel error -rtsp_transport tcp \
		-i "$URL" -frames:v 1 -q:v 3 -y "$file" 2>&1 \
		| sed -E 's#(rtsps?://)[^@/]*@#\1***@#g'
	[ -s "$file" ]
}

if out=$(grab) || { sleep 20; out=$(grab); }; then
	log "ok $file $(stat -c %s "$file") bytes"
else
	log "ERROR: snapshot failed: ${out:-no output}"
	rm -f "$file"
	exit 1
fi

find "$OUT" -mindepth 1 -maxdepth 1 -type d -name '20*' -mtime +$KEEP_DAYS -exec rm -rf {} +
