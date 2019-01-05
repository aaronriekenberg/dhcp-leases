#!/bin/sh

cd /home/aaron/dhcp-leases

if [ ! -f ./oui.txt ]; then
  ./fetch_oui.sh
fi

if [ ! -f ./oui.db ]; then
  ./make-db
fi

echo "Now: $(date)"
./show-leases
