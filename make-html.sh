#!/bin/sh

OUTPUT_FILE=/var/www/htdocs/dhcp/leases.html

cd /home/aaron/dhcp-leases

cat top.html > $OUTPUT_FILE
./show-leases.sh 2>&1 | recode 'utf8..html' >> $OUTPUT_FILE
cat bottom.html >> $OUTPUT_FILE
