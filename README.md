# dhcp-leases

C version of dhcp lease info printer for OpenBSD.  

Motivated by the fact that ISC's dhcp-lease-list.pl does a grep through the 146K line oui.txt file for every dhcp lease and is slow on low-end hardware.

## make-db 
reads oui.txt and writes oui.db (a Berkeley BTree DB).

## show-leases 
reads /var/db/dhcpd.leases and oui.db and displays the results.  show-leases runs in 17ms on a 1.6ghz Atom 330 running OpenBSD 6.2 with 19 leases in use.

## dhcp-leases
CGI app for use with httpd and slowcgi.  Installation steps for dhcp-leases:

    1. cp dhcp-leases /var/www/cgi-bin/
    2. chmod 0555 /var/www/cgi-bin/dhcp-leases
    3. cp oui.db /var/www/conf/
    4. chmod 0644 /var/www/conf/oui.db
    5. ln /var/db/dhcpd.leases /var/www/conf/dhcpd.leases
