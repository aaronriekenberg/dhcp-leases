# dhcp-leases

C version of dhcp lease info printer.  

Motivated by the fact that ISC's dhcp-lease-list.pl does a grep through the 146K line oui.txt file for every dhcp lease and takes around 1 second on low-end hardware.

make-db reads oui.txt and writes oui.db (a Berkley BTree DB).

show-leases reads /var/db/dhcpd.leases and oui.db and displays the results.  show-leases runs in 17ms on a 1.6ghz Atom 330 running OpenBSD 6.2 with 19 leases in use.

Example of show-leases:

```
reading /var/db/dhcpd.leases
dbFileName = oui.db

IP                End Time                    MAC                 Hostname                Organization
====================================================================================================================
192.168.0.10      2018/03/03 00:08:02 -0600   14:99:e2:2b:f8:bb   Apple-TV-4              Apple, Inc.
192.168.0.11      2018/03/04 16:22:58 -0600   14:99:e2:2b:f8:bc   Apple-TV-4              Apple, Inc.
192.168.0.14      2018/03/04 18:14:00 -0600   0c:47:c9:f3:f1:d3   NA                      Amazon Technologies Inc.
192.168.0.15      2018/03/04 17:56:09 -0600   8c:85:90:1d:35:99   Aarons-MBP              Apple, Inc.
192.168.0.16      2018/03/04 17:32:11 -0600   00:1f:32:5e:81:d4   Wii                     Nintendo Co., Ltd.
192.168.0.17      2018/03/04 18:19:03 -0600   f0:9f:c2:2c:e9:e5   NA                      Ubiquiti Networks Inc.
192.168.0.18      2018/03/04 17:10:50 -0600   78:8a:20:56:79:8e   NA                      Ubiquiti Networks Inc.
192.168.0.19      2018/03/04 17:56:43 -0600   a4:b8:05:d4:09:2a   Aarons-iPhone           Apple, Inc.
192.168.0.20      2018/03/04 18:17:09 -0600   6c:72:e7:5c:b8:cd   Tracys-iPhone           Apple, Inc.
192.168.0.21      2018/03/03 08:13:06 -0600   a0:99:9b:06:b2:c9   a0999b06b2c9            Apple, Inc.
192.168.0.22      2018/03/04 13:38:06 -0600   d8:6c:63:65:1b:04   Chromecast              Google, Inc.
192.168.0.23      2018/03/04 18:12:04 -0600   1c:9e:46:d8:67:9c   Tracys-iPad             Apple, Inc.
192.168.0.24      2018/03/04 17:56:03 -0600   e4:98:d6:f3:14:14   iPhone                  Apple, Inc.
192.168.0.25      2018/03/04 17:17:58 -0600   f8:da:0c:7c:88:97   BRWF8DA0C7C8897         Hon Hai Precision Ind. Co.,Ltd.
192.168.0.30      2018/03/04 17:11:06 -0600   2c:1d:b8:9d:de:7b   DIRECTV-HR54-B89DDE7A   ARRIS Group, Inc.
192.168.0.31      2018/03/04 17:43:55 -0600   e0:66:78:3b:ff:09   Tracys-iPad-3           Apple, Inc.
192.168.0.32      2018/03/04 18:00:45 -0600   80:e6:50:25:a6:66   Tracys-MBP              Apple, Inc.
192.168.0.33      2018/03/04 16:53:21 -0600   10:62:d0:07:1a:ff   NA                      Technicolor CH USA Inc.
192.168.0.34      2018/03/04 17:10:15 -0600   10:62:d0:07:16:63   NA                      Technicolor CH USA Inc.

19 IPs in use
```
