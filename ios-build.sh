#!/bin/sh
# use makefile -G "Unix Makefiles"
# use xcode -G Xcode -T "buildsystem=1"
# cmake --build ios_arm64 --config Release

COMPILE_ARCHS=("arm64" "x86_64")

SCRIPT_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
OUTDIR="$SCRIPT_PATH/out"

if [ ! `which yasm` ]
then
    echo 'Yasm not found'
    if [ ! `which brew` ]
    then
        echo 'Homebrew not found. Trying to install...'
                    ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)" \
            || exit 1
    fi
    echo 'Trying to install Yasm...'
    brew install yasm || exit 1
fi
if [ ! `which gas-preprocessor.pl` ]
then
    echo 'gas-preprocessor.pl not found. Trying to install...'
    (curl -L https://github.com/libav/gas-preprocessor/raw/master/gas-preprocessor.pl \
        -o /usr/local/bin/gas-preprocessor.pl \
        && chmod +x /usr/local/bin/gas-preprocessor.pl) \
        || exit 1
fi

rm -rf $OUTDIR/*
mkdir $OUTDIR/x265

for arch in ${COMPILE_ARCHS[@]}
do
    case $arch in
    arm64)
        ASM_COMPILER="gas-preprocessor.pl -arch aarch64 -- xcrun -sdk iphoneos clang -O3 -arch arm64"
        IOS_PLATFORM="OS64"
        SYSROOT="/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk"
        ;;
    armv7)
        ASM_COMPILER="gas-preprocessor.pl -arch arm -- xcrun -sdk iphoneos clang -O3 -arch armv7"
        IOS_PLATFORM="OS"
        SYSROOT="/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk"
        ;;
    x86_64)
        IOS_PLATFORM="SIMULATOR64"
        SYSROOT="/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk"
        ;;
    x86)
        IOS_PLATFORM="SIMULATOR"
        SYSROOT="/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk"
        ;;
    esac

    BUILD_PATH="${OUTDIR}/${arch}"

    cmake source -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=${SCRIPT_PATH}/ios.toolchain.cmake -DPLATFORM=${IOS_PLATFORM} -B $BUILD_PATH \
    -DCMAKE_INSTALL_PREFIX=$BUILD_PATH \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DCMAKE_SYSROOT="$SYSROOT" \
    -DCMAKE_BUILD_TYPE=RELEASE \
    -DENABLE_PIC=ON \
    -DENABLE_CLI=OFF \
    -DENABLE_BITCODE=NO \
    -DDEPLOYMENT_TARGET=9.0 \
    -DCMAKE_ASM_NASM_FLAGS=-w-macro-params-legacy \
    -DENABLE_SHARED=OFF \
    -DASM_COMPILER="$ASM_COMPILER" \
    # -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=7J5XWXBPXY \

    pushd $BUILD_PATH
    make -j4 && make install
    popd
done

MERGE_CMD="lipo -create "
for arch in ${COMPILE_ARCHS[@]}
do
    if [ -d "$OUTDIR/$arch" ]; then
        MERGE_CMD="$MERGE_CMD $OUTDIR/$arch/lib/libx265.a"
        cp -r $OUTDIR/$arch/include $OUTDIR/x265
    fi
done

MERGE_CMD="$MERGE_CMD -output $OUTDIR/x265/libx265.a"

echo "$MERGE_CMD"
$($MERGE_CMD)