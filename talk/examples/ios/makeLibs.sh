#!/bin/bash -e

if [ "$(dirname $0)" != "." ]; then
  echo "$(basename $0) must be run from talk/examples/ios as ./$(basename $0)"
  exit 1
fi

rm -rf libs
mkdir libs

cd ../../../xcodebuild/Debug-iphoneos
for f in *.a; do
  if [ -f "../Debug-iphonesimulator/$f" ]; then
    echo "creating fat static library $f"
    lipo -create "$f" "../Debug-iphonesimulator/$f" -output "../../talk/examples/ios/libs/$f"
  else
    echo ""
    echo "$f was not built for the simulator."
    echo ""
  fi
done

cd ../Debug-iphonesimulator
for f in *.a; do
  if [ ! -f "../Debug-iphoneos/$f" ]; then
    echo ""
    echo "$f was not built for the iPhone."
    echo ""
  fi
done
