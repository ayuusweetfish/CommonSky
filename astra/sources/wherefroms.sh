#!/bin/bash
# Based on https://github.com/jakepetroules/wherefrom
set -o pipefail
for file in ../img/*; do
  contents=$(xattr -p com.apple.metadata:kMDItemWhereFroms "$file")
  if [ "$?" == 0 ]; then
    xxd -r -p <<< "$contents" \
      | plutil -convert xml1 -o - - \
      | grep "string" \
      | cat <(echo $(basename $file)) -
  fi
done
