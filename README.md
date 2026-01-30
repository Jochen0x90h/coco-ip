# CoCo IP

IP protocol module for CoCo.
Defines basic address structures and provides socket abstractions.
Implements UDP/IP and TCP/IP client on native platforms. Additionally import coco-tcp for TCP/IP servers.

## Import
Add coco-ip/\<version> to your conanfile where version corresponds to the git tags.

## Features
* Connection based IP socket (UDP with fixed destination address or TCP client)
* Connectionless UDP socket with multicast

## Supported Platforms
* Native
  * Windows
