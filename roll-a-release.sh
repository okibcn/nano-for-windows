#!/bin/bash

VERSION="9.0"

./configure -C --enable-tiny &&  make &&  ./configure -C &&

echo "Running autogen..." &&  ./autogen.sh &&
rm -v -f m4/*.m4~ *.asc *.sig *.gz *.xz &&

echo "Rebuilding..." &&  make &&
po/update_linguas.sh &&

make distcheck &&  make dist-xz &&

git add po/*.po po/nano.pot po/LINGUAS &&
git commit -m "$(git log -1 --grep 'po: up' | grep o: | sed 's/^    //')" &&

gpg -a -b nano-$VERSION.tar.gz &&
gpg -a -b nano-$VERSION.tar.xz &&
gpg --verify nano-$VERSION.tar.gz.asc &&
gpg --verify nano-$VERSION.tar.xz.asc &&

git tag -u B8E1961F -a v$VERSION -m "the nano $VERSION release" &&

make pdf &&  rm -rf doc/nano.t2p &&
scp doc/nano.pdf bens@wh0rd.org:nano.pdf &&
scp doc/cheatsheet.html bens@wh0rd.org:cheatsheet.html &&

for file in nano-$VERSION.tar.*z*; do scp $file bens@wh0rd.org:$file; done &&

gnupload --to ftp.gnu.org:nano  nano-$VERSION.tar.*z &&

echo "Tarballs have been rolled and uploaded."
