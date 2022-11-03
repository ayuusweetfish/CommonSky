#!/bin/bash
# Based on https://github.com/jakepetroules/wherefrom
set -o pipefail
for file in ../img/*; do
  xattr -p com.apple.metadata:kMDItemWhereFroms "$file" \
    | xxd -r -p \
    | plutil -convert xml1 -o - - \
    | grep "string" \
    | cat <(echo $(basename $file)) -
done
