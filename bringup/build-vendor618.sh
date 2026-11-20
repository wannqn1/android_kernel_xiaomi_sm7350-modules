#!/bin/bash
# Build redwood's out-of-tree vendor modules against the 6.18 kernel in
# ~/r618/out. Each tree builds in its own copy under ~/r618/vb, logs go to
# ~/r618/vb/<name>.log. Usage: build-vendor618.sh [tree ...]
# Vendor trees never get out/Module.symvers in KBUILD_EXTRA_SYMBOLS: modpost
# already reads it through O=, passing it again makes every symbol "exported twice".
R=${R:-$HOME/r618}; K=$R/redwood-6.18-kernel; O=$R/out; V=$R/redwood-6.18-modules; W=$R/vb
B=$V/bringup
export PATH=${CLANG:-$HOME/tc/clang-r596125/bin}:$PATH
MK="make -C $K O=$O ARCH=arm64 LLVM=1 LLVM_IAS=1 LOCALVERSION= ${PAHOLE:+PAHOLE=$PAHOLE} -j${JOBS:-$(nproc)}"
mkdir -p $W
trees=${*:-"mm-drivers display-drivers graphics-kernel securemsm-kernel audio-kernel"}
EXT=$V/mm-drivers/msm_ext_display/include
FENCE="-I$W/mm-drivers/hw_fence/include -I$W/mm-drivers/sync_fence/include -I$EXT"
mmsyms() { ls $W/mm-drivers/*/Module.symvers 2>/dev/null | tr '\n' ' '; }
for t in $trees; do
	rsync -a --delete $V/$t/ $W/$t/
	case $t in
	mm-drivers)
		# msm_ext_display's own Kbuild wants the Bazel config headers; build the one file directly
		printf 'ccflags-y += -DCONFIG_MSM_EXT_DISPLAY=1 -I$(src)/include\nobj-m += msm_ext_display.o\nmsm_ext_display-y := src/msm_ext_display.o\n' \
			> $W/mm-drivers/msm_ext_display/Kbuild
		for d in msm_ext_display sync_fence hw_fence; do
			$MK M=$W/mm-drivers/$d CONFIG_ARCH_LAHAINA=y CONFIG_ARCH_YUPIK=y \
				SYNC_FENCE_ROOT=$W/mm-drivers/ MSM_HW_FENCE_ROOT=$W/mm-drivers/ \
				CONFIG_MSM_EXT_DISPLAY=m CONFIG_QCOM_SPEC_SYNC=m CONFIG_QTI_HW_FENCE=m \
				KCFLAGS="$FENCE" modules > $W/mm-$d.log 2>&1
			echo "mm-drivers/$d rc=$?"
		done ;;
	display-drivers)
		$MK M=$W/display-drivers DISPLAY_ROOT=$W/display-drivers KBUILD_EXTRA_SYMBOLS="$(mmsyms)" \
			CONFIG_ARCH_LAHAINA=y CONFIG_ARCH_YUPIK=y CONFIG_DRM_MSM=m \
			SOCINCLUDE="$FENCE" modules > $W/display.log 2>&1
		echo "display-drivers rc=$?" ;;
	graphics-kernel)
		$MK M=$W/graphics-kernel KGSL_PATH=$W/graphics-kernel KBUILD_EXTRA_SYMBOLS="$(mmsyms)" \
			CONFIG_ARCH_LAHAINA=y CONFIG_ARCH_YUPIK=y KCFLAGS="-I$K/drivers/devfreq $FENCE" \
			SOCINCLUDE="$FENCE" modules > $W/gpu.log 2>&1
		echo "graphics-kernel rc=$?" ;;
	securemsm-kernel)
		$MK M=$W/securemsm-kernel SSG_MODULE_ROOT=$W/securemsm-kernel \
			CONFIG_ARCH_LAHAINA=y CONFIG_DISABLE_TEST_MODULES=y CONFIG_QSEECOM=m modules > $W/securemsm.log 2>&1
		echo "securemsm-kernel rc=$?" ;;
	audio-kernel)
		# audio-kernel only ships a Bazel registry for lahaina; generate a Kbuild from it
		rm -f $W/audio-kernel/Makefile
		python3 $B/audiokbuild.py $W/audio-kernel
		$MK M=$W/audio-kernel KBUILD_EXTRA_SYMBOLS="$W/mm-drivers/msm_ext_display/Module.symvers" \
			KCFLAGS="-I$EXT" modules > $W/audio.log 2>&1
		echo "audio-kernel rc=$?" ;;
	esac
done
for f in $W/*.log; do
	n=$(grep -cE "error:" $f)
	echo "$(basename $f): $n errors"
done
