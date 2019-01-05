#!/bin/sh

OUTPUT_FILE=/var/www/htdocs/dhcp/leases.html

cd /home/aaron/dhcp-leases

cat top.html > $OUTPUT_FILE
./show-leases.sh >> $OUTPUT_FILE 2>&1
cat bottom.html >> $OUTPUT_FILE
