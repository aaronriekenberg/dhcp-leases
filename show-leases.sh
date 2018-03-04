#!/bin/sh

cd /home/aaron/dhcp-leases

if [ ! -f ./oui.db ]; then
  ./make-db
fi

./show-leases
