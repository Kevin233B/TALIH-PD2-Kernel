# TALIH-PD2_Kernel

如你所见，这里如你所见

这是一个由TALPAD-BOOM开发团队维护的TALPAD设备的Linux kernel，对，你没看错，就是那个Linux kernel，我们打算给TALPAD维护内核

但是TAL（就是那个TAL）似乎给这内核动了点手脚，大概内容为：

1. 有些.c指向的头文件地址稀烂，本应写<目标头文件>却写成了"目标头文件"

2. 有些内容似乎是闭源的，随便TAL了，反正也是他们自己写的东西，该开源的还是开源了

3. 连kconfig都有些错误，刚开始编译时简直是开幕雷击

我们打算在n个月内给该内核适配sukisu/kernelsu

---

SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note

Copyright (C) 1991-2026 Linus Torvalds and the Linux Kernel Community (original Linux kernel)

Copyright (C) 2026 TAL (TALPAD-BOOM) (modified distribution)

本仓库基于Linux内核源码修改发布，原始版权归Linux内核社区所有，修改部分版权归TALPAD-BOOM开发团队所有

# 怎么编译这坨屎？

TALPAD的/proc/version记录的编译环境是llvm-r383902

因此我特地准备了编译用的clang和lld

[llvm-clang](https://productionresultssa2.blob.core.windows.net/actions-results/ae59f28c-e33f-40f0-ae12-dc3ea0444422/workflow-job-run-3aa1c764-7008-5084-ab38-88585e421f62/artifacts/bc23bd25828b24211442c4eb5b6e9f59fd416cb7e571bb16de26a8bed694bc4c.zip?rscd=attachment%3B+filename%3D%22llvm-clang.zip%22&rsct=application%2Fzip&se=2026-07-01T12%3A57%3A49Z&sig=LPfeOfgKzDaa5ke3grzkU2%2F7w%2B8zTb526QtmUpGWf%2BQ%3D&ske=2026-07-01T15%3A16%3A51Z&skoid=ca7593d4-ee42-46cd-af88-8b886a2f84eb&sks=b&skt=2026-07-01T11%3A16%3A51Z&sktid=398a6654-997b-47e9-b12b-9515b896b4de&skv=2025-11-05&sp=r&spr=https&sr=b&st=2026-07-01T12%3A47%3A44Z&sv=2025-11-05)