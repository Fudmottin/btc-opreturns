#!/bin/bash

for txtfile in *.txt; do
    [ -f "$txtfile" ] || continue
    binfile="${txtfile%.txt}.bin"
    xxd -r -p "$txtfile" > "$binfile"
    file "$binfile"
done

