# TALIH-PD2_Kernel

基本名词详解：

1.TAL：好未来（包括旗下的学而思）

2.TALPAD：学而思学习机（学而思平板;好未来平板）

3.TALPAD-BOOM：由Public class 早茶光于2024年3月18日组建的TALPAD讨论群，并非TAL附属组织，其核心理念是“自由、开放与安全”，于2025年5月1日转移主权，其管理层仍处于核心地位，目前Public class 早茶光已不再参与主要破解/管理/开发工作，但其带来的影响力不可忽视

4.内核Root：分为两大类，第一种是直接依靠外部修补的root管理器，如“FolkPatch”“APatch”，第二种是内核集成式root，如SukiSu，KernelSU

5.GKI：通用内核映像，是Google自Kernel 5.4以来开始施行的统一开发管理，为解决不同设备、不同厂商内核源码的碎片化

6.TALPAD-Kernel-Team：负责管理此内核源码的直属组织，衍生于TALPAD-BOOM，但由于TALPAD-BOOM人均技术水平不足3.8分（满分十分）而迟迟无法找到更多开发者

---

如你所见，这里如你所见

这是一个由TALPAD-BOOM开发团队维护的TALPAD设备的Linux kernel，对，你没看错，就是那个Linux kernel，我们打算给TALPAD维护内核

~~但是TAL（就是那个TAL）似乎给这内核动了点手脚~~，大概内容为：

1. ~~有些.c指向的头文件地址稀烂，本应写<目标头文件>却写成了"目标头文件"~~（已解决）

2. ~~有些内容似乎是闭源的，随便TAL了，反正也是他们自己写的东西，该开源的还是开源了~~（错觉来的）

3. ~~连kconfig都有些错误，刚开始编译时简直是开幕雷击~~（已解决）

我们打算在n个月内给该内核适配sukisu/kernelsu

---

SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note

Copyright (C) 1991-2026 Linus Torvalds and the Linux Kernel Community (original Linux kernel)

Copyright (C) 2026 TAL (TALPAD-BOOM) (modified distribution)

本仓库基于Linux内核源码修改发布，原始版权归Linux内核社区所有，修改部分版权归TALPAD-BOOM开发团队所有

# 怎么编译内核？~~再也不是屎了~~

TALPAD的/proc/version记录的编译环境是llvm-r383902

因此我特地准备了编译用的clang和lld

[Download llvm-tools](https://github.com/Kevin233B/TALIH-PD2-Kernel/releases/download/llvm-r383902b/llvm-tools.zip)

我已经把需要的东西放在prebuilts里面了 amd64开箱即用

暂不支持arm64 

我准备了一些脚本，如下：

1.~~执行Integrate_sukisu.sh~~（等SUSFS支持之后吧）

2.~~执行Integrate_SUSFS.sh~~（没兼容）

3.执行download_clang.sh

4.最后执行build-mt8797.sh

注意：这些脚本目前还处于开发阶段，因此可能会造成一些意外的问题

如果你需要sukisu/ksu，请自行修改defconfig

# Other

TALPAD-Kernel-Team将会使用ai+人工的方式扫描内核源码中的漏洞，致力使此内核更加安全，避免恶意程序损坏用户设备

By TALPAD-BOOM Kevin233
