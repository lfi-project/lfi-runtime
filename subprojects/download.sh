#!/bin/sh

set -ex

git clone https://github.com/argtable/argtable3 -b v3.2.2.f25c624
rm -rf argtable3/.git

git clone https://github.com/likle/cwalk -b v1.2.9
rm -rf cwalk/.git

git clone https://github.com/aengelke/fadec
rm -rf fadec/.git

git clone https://github.com/aengelke/disarm
rm -rf disarm/.git

git clone https://github.com/lfi-project/lfi-verifier
rm -rf lfi-verifier/.git

git clone https://github.com/zyedidia/libboxmap
rm -rf libboxmap/.git

git clone https://github.com/zyedidia/libelf-bsd
rm -rf libelf-bsd/.git

git clone https://github.com/zyedidia/libmmap -b mmap-simple
rm -rf libmmap/.git

cp packagefiles/cwalk/meson.build ./cwalk
cp packagefiles/argtable3/meson.build ./argtable3
cp packagefiles/lfi-verifier/meson.build ./lfi-verifier
cp lfi-verifier/subprojects/packagefiles/disarm/meson.build ./disarm
cp lfi-verifier/subprojects/packagefiles/fadec/meson.build ./fadec
