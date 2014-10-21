#!/bin/sh

# make sure that m4 directory exists before call to autoreconf
mkdir -p m4
autoreconf --install
