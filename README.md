RTOnBootEthercat

The RTOnBoot framework is an industry-leading framework for multi-core heterogeneous coexistence of Linux and RTOS. One type of heterogeneous architecture is Linux or Android, and the other is RTOS. The RTOnBoot framework requires corresponding modifications to the bootloader, Linux or Android, and RTOS. The current version of the RTOS uses Nuttx.
Neither Xenomai nor Preempt-RT has a cache isolation design like RTOnBoot, allowing for free variable definition. This results in significant cache thrashing, making their real-time performance and stability inferior to RTOnBoot. This is determined by the ARM64 IP. Furthermore, while some RTOSs may offer better real-time performance than RTOnBoot's Nuttx, programs like AI and Linuxcnc have extensive logic in Linux user space, making them difficult to port to these pure RTOSs.
Please see the document doc/rtonboot_introduction_en.docx .



This open-source base version is a single-core, single-master version, containing a simplified version of the RTOnBoot framework. The single core controlled by Nuttx is configurable in U-Boot and the Linux kernel, but Nuttx timers are hard-coded, although all source code is available and users can modify them. The physical address of the shared buffer, etc., is also hard-coded, but all source code is available and users can modify it. The C++ library is a slightly older version of C++ library that does not support C++ exceptions, but it is sufficient for most needs, as C++ exceptions are generally not used in the embedded field. Other source code includes the porting of the SOEM protocol stack, the real-time network card driver for RK's built-in STMMAC network card, the corresponding Linux kernel driver source code, including a virtual real-time gateway network card driver, rtbridge, rtbulk, etc., and all Nuttx code and Nuttx apps code are also open source. All applications and libraries under Buildroot are also open source. Uboot and kernel are also fully open source. ICOS, i.e., IGH Commands Over SOEM, is also completely open source. 

Please see the document doc/rtonboot_ethercat_en.docx。



The basic version uses the RK3588 CPU, and the experimental environment is the RK3588 development board from Rongpin Electronics. However, the modifications made by RTOnBoot are generally based on the public code of Linux, Uboot, or RK, so the sensitivity to the board is very small, and users with other boards can easily port it. Unlike previous open-source projects that required purchasing a board, this one has undergone thorough testing, and users can confidently verify it. It is more than sufficient for a single-core, single-master station.



The `readme.docx` file in the `doc` directory contains a basic guide for compiling, porting, and developing. Nuttx, U-Boot, and the kernel have all their source code committed. The buildroot source code is in the `buildroot/work` directory, containing libraries and applications. There are also corresponding directories in the `buildroot/package` directory. Configuration changes have been made to `buildroot/package/Config.in` and `buildroot/configs/rockchip_rk3588_defconfig`. Other code has not been committed because our code does not depend on other code in buildroot; it's all public RK code. You can request the code from Rongpin or download it from the public RK repository. The professional version will have a higher dependency rate. The Nuttx Toolchain can not upload, users can download from network, the version is aarch64-none-elf-gcc (Arm GNU Toolchain 11.3.Rel1) 11.3.1 20220712.



The RTOnBoot framework uses a dual-licensing system. The basic version uses a GPLv2 license, while there is also a professional version with a commercial license. It is also fully open source, but requires a small fee; details can be discussed.



The professional version has significantly more features than the basic version. It is a multi-core version, but can also be configured as a single-core. CPUID and timers are dynamically configurable, and the physical address of shared memory is also dynamically obtained, making porting easy. The C++ library is a high-version C++ library that supports C++ exceptions. It features multi-core, multi-master functionality, allowing multiple SOEM masters to run on multiple cores, maximizing the performance of each master—a feature not found in native SOEM. It can also be configured as a single master. The professional version also includes a full-featured OpenPLC implementation and a full-featured Linuxcnc implementation. Please see the documents doc/rtonboot_openplc_en.docx and doc/rtonboot_linuxcnc_en.docx .
Therefore, interested users are encouraged to upgrade to the professional version as soon as possible.

Contact Person: Winfred Young  Email: winfredy888@163.com

