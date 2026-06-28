#!/bin/sh

cd "$(dirname "$0")" || exit 1

./format.sh || exit 1

git diff

echo
echo "commit?y=yes"
read x
if [ "$x" "=" y ]; then
  git commit -as
fi
