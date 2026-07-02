#!/bin/bash -e

source "${RK_POST_HELPER:-$(dirname "$(realpath "$0")")/../post-hooks/post-helper}"

OS_RELEASE="$TARGET_DIR/etc/os-release"

fixup_os_release()
{
	KEY=$1
	shift

	sed -i "/^$KEY=/d" "$OS_RELEASE"
	echo "$KEY=\"$@\"" >> "$OS_RELEASE"
}


function write_release_version()
{
    GIT_COMMIT=`git log --oneline -1 | awk {'print $1'}`
    SDK_VERSION=`git describe --tags $(git rev-list --tags --max-count=1)`
    BUILD_TIME=`date +"%Y-%m-%d %H:%M:%S"`

{
    echo "#!/bin/bash"
    echo "echo ======================================================================="
    echo "echo kernel info: \`cat /proc/version\`"
    echo "echo image build time: $BUILD_TIME"
    echo "echo SDK latest commit: $GIT_COMMIT"
    echo "echo \"SDK version: $SDK_VERSION\""
    echo "echo ======================================================================="
    echo
    echo "echo Rootfs info:"
    echo "echo \"\`cat /etc/os-release\`\""
} > ${TARGET_DIR}/usr/bin/release_version

    chmod a+x ${TARGET_DIR}/usr/bin/release_version
}

message "Adding information to /etc/os-release..."

mkdir -p "$(dirname "$OS_RELEASE")"
[ -f "$OS_RELEASE" ] || touch "$OS_RELEASE"

BUILD_INFO="$(whoami)@$(hostname) $(date)"
case "$POST_OS" in
	buildroot) BUILD_INFO="$BUILD_INFO - $RK_BUILDROOT_CFG" ;;
	yocto) BUILD_INFO="$BUILD_INFO - ${RK_YOCTO_MACHINE:-$RK_YOCTO_CFG}" ;;
esac

fixup_os_release ID_LIKE "$POST_OS"
fixup_os_release RK_BUILD_INFO "$BUILD_INFO"

if [ "$POST_ROOTFS" ]; then
	cp -f "$OS_RELEASE" "$RK_OUTDIR"
fi

message "Adding release_version to get version info"
write_release_version
