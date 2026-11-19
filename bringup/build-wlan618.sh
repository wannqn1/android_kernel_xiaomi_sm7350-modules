#!/bin/bash
# Build redwood Wi-Fi (WCN6750 on the WPSS) against the 6.18 kernel in ~/r618/out:
# the platform drivers (icnss2, cnss_utils, cnss_nl, cnss_prealloc,
# wlan_firmware_service) and qcacld (wlan.ko, profile lahaina_gki_qca6750).
R=${R:-$HOME/r618}; K=$R/redwood-6.18-kernel; O=$R/out; V=$R/redwood-6.18-modules; W=$R/vb
export PATH=${CLANG:-$HOME/tc/clang-r596125/bin}:$PATH
MK="make -C $K O=$O ARCH=arm64 LLVM=1 LLVM_IAS=1 LOCALVERSION= ${PAHOLE:+PAHOLE=$PAHOLE} -j${JOBS:-$(nproc)}"
rsync -a --delete $V/wlan/ $W/wlan/
P=$W/wlan/platform
profile=${WLAN_PROFILE:-lahaina_gki_qca6750}
platform=CONFIG_ICNSS2=m
if [ "$profile" = lahaina_gki_qca6490 ]; then
	platform=CONFIG_CNSS2=m
fi
$MK M=$P "$platform" CONFIG_ICNSS2_QMI=y CONFIG_CNSS2_QMI=y CONFIG_CNSS_UTILS=m CONFIG_CNSS_GENL=m \
	CONFIG_WCNSS_MEM_PRE_ALLOC=m CONFIG_CNSS_QMI_SVC=m CONFIG_CNSS_OUT_OF_TREE=y \
	KCFLAGS="-I$P/inc -I$P/cnss_utils -I$P/cnss2 -I$P/icnss2 -I$P/cnss_prealloc" modules > $W/wlanplat.log 2>&1
echo "wlan platform rc=$?"
[ "$1" = platform ] && exit 0
Q=$W/wlan/qcacld-3.0
$MK M=$Q WLAN_ROOT=$Q WLAN_PROFILE="$profile" MODNAME=wlan BUILD_DIAG_VERSION=y \
	CONFIG_QCA_CLD_WLAN=m CONFIG_QCA_WIFI_ISOC=0 CONFIG_QCA_WIFI_2_0=1 QCA_WIFI_FTM_NL80211=y \
	KBUILD_EXTRA_SYMBOLS=$P/Module.symvers \
	KCFLAGS="-I$P/inc -I$W/wlan/inc-net -include $Q/configs/kbuild_defines.h -include $Q/configs/config_to_feature.h -include $Q/configs/grep_defines_lahaina.h -Wno-macro-redefined -DWLAN_OPEN_SOURCE -Wno-error=missing-prototypes -Wno-error=unused-variable -Wno-error=unused-but-set-variable" \
	modules > $W/qcacld.log 2>&1
echo "qcacld rc=$?"
for f in wlanplat qcacld; do echo "$f: $(grep -c "error:" $W/$f.log) errors"; done
find $W/wlan -name "*.ko" | xargs -n1 basename
