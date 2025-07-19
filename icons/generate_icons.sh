#!/bin/bash
cd `dirname $0`
for n in download eject insert mkdir new_image reload trash upload; do
	./bin2h ${n}_icon.png ${n}_icon > ../uc/src/${n}_icon.h
done
./bin2h favicon.png favicon > ../uc/src/favicon.h
