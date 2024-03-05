# HDMI Peripheral: Video Player

This is a user-space application that drives the HDMI Peripheral for the Zynq
7000 defined by [hdmi-cmd-gen][1] and [hdmi-cmd-enc][2]. Taking a video file as
input, this program decodes the video using `libav`, then plays the decoded
stream using the device.

Video files must be 640x480, they must have pixels encoded as
`AV_PIX_FMT_YUV420P`, and they can only have one stream. I didn't implement any
logic to handle the other cases.

## Usage

This program expects positional command-line arguments for:
1. the video file to play `[VIDEO]`, and
2. the frame rate divider `[FDIV]`, which has `60Hz / [FDIV] = Frame Rate` and
   which must be an integer.

Additionally, this application uses the HDMI Peripheral. It expects to be
running on a Zynq 7000 platform, and it needs to be able to program the PL via
the `sysfs` interface mentioned on [Confluence][3]. It also needs to be able to
access device memory via `/dev/mem`, and it has to be able use the [ZOCL][4]
interface on the `platform-axi:zyxclmm_drm-render` DRI device.

Finally, the `.bit` file for the HDMI Peripheral needs to be converted to a
`.bin` file, and the result needs to be placed at `/lib/firmware/hdmi_dev.bin`.
The `scripts/bit2bin.py` script can be used to perform the conversion. This
program expects to find the AXI4-Lite interface to the device at `0x4000_0000`.

[1]: https://github.com/ammrat13/hdmi-cmd-gen "ammrat13/hdmi-cmd-gen"
[2]: https://github.com/ammrat13/hdmi-cmd-enc "ammrat13/hdmi-cmd-enc"
[3]: https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18841847/Solution+ZynqMP+PL+Programming#SolutionZynqMPPLProgramming-FPGAprogrammingusingsysfsattributes "Solution ZynqMP PL Programming: FPGA programming using sysfs attributes"
[4]: https://xilinx.github.io/XRT/2023.2/html/zocl_ioctl.main.html "Xilinx Runtime Architecture: ZOCL Driver Interfaces"
