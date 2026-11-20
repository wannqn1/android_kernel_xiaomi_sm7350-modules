# Renoir CVP and SYNX

The modules preserve the Renoir 5.4 ioctl ABI and packet formats. The port uses
6.18 DMA heaps, IOMMU and interconnect APIs, and loads the Renoir EVASS firmware
through Qualcomm PAS.

Donor source is [Failedmush1/android_kernel_qcom_sm8350 at 9d60f57](https://github.com/Failedmush1/android_kernel_qcom_sm8350/tree/9d60f57e2f7868681dbb3477bfa064a0cfef4ed6/drivers/media/platform/msm).
The original CVP, SYNX, and UAPI files retain their license notices.

After building the base kernel and modules, run
`redwood-6.18-kernel/renoir/build-tools/build-renoir-kernel.sh cvp` in the
Renoir build workspace. It stages both modules and the CVP-enabled DTB under
`work/renoir-build/cvp-test/device/xiaomi/renoir-kernel-6.18`.
The usual `stage` target produces the base package. CVP needs handset
validation for EVASS startup, CDSP messaging, and camera sessions.
