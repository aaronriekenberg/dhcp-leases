# dhcp-leases

C version of dhcp lease info printer.  

Motivated by the fact that ISC's dhcp-lease-list.pl does a grep through the 146K line oui.txt file for every dhcp lease and is slow on low-end hardware.

make-db reads oui.txt and writes oui.db (a Berkeley BTree DB).

show-leases reads /var/db/dhcpd.leases and oui.db and displays the results.  show-leases runs in 17ms on a 1.6ghz Atom 330 running OpenBSD 6.2 with 19 leases in use.
