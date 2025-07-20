#!/bin/sh

function build {
    foco65 -p '$3F00' $1.forth > /tmp/$1.asx || exit 1
    xasm /l /tmp/$1.asx /o:/tmp/$1.xex || exit 1
}

cd `dirname $0`
build yasio_setup
