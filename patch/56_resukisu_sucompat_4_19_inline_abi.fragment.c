/*
 * patch/56_resukisu_sucompat_4_19_inline_abi.fragment.c
 *
 * 由 CI 在 setup.sh 克隆 ReSukiSU 之后，追加到
 * ReSukiSU/kernel/feature/sucompat.c 末尾（cat >>，纯增量，上游一行不改）。
 *
 * 背景：ReSukiSU main（faccf4c5 之后）把 ksu_handle_faccessat /
 * ksu_handle_stat 的 CONFIG_KSU_SUSFS inline 变体签名从
 *     const char __user **filename_user   （单次解引用，4.19 调用点 ABI）
 * 改成
 *     struct filename **filename          （二次解引用，6.1+ getname 调用点 ABI）
 * 且把 faccf4c5 原有的 "#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0) &&
 * defined(CONFIG_KSU_SUSFS)" 版本门收窄为纯 "#ifdef CONFIG_KSU_SUSFS"。于是
 * 4.19 + SUSFS 的构建会编入 struct filename ** 变体，而 4.19 的
 * do_faccessat/vfs_statx 调用点传入的是 &filename（char **），驱动二次解引用
 * 把路径字符串字节当指针，kernel_init 阶段 ksys_access("/init") 即 data abort
 * 变砖（pc=ksu_handle_faccessat+0x70，lr=do_faccessat+0x54，ESR 0x96000004，
 * fault 地址内容 "/init\0fs"）。
 *
 * 修复：保留 main 全部内容，在文件末尾追加这两个 char ** ABI 的兼容函数
 * （_*_4_19 命名，实现取自 main 自身非 SUSFS 分支），树内 fs/open.c、fs/stat.c
 * 调用点改调新名。上游若继续漂移，本片段只可能编译失败（CI 变红），
 * 不可能静默带病开机。
 */

int ksu_handle_faccessat_4_19(int *dfd, const char __user **filename_user, int *mode, int *__unused_flags)
{
    char path[sizeof(su_path) + 1] = { 0 };
    const struct cred *old_cred;

#ifndef CONFIG_KSU_TRACEPOINT_HOOK
    if (ksu_is_current_proc_unprivillege()) {
        return 0;
    }
#endif

#ifdef KSU_COMPAT_USE_STATIC_KEY
    if (!static_branch_unlikely(&ksu_su_compat_enabled)) {
        return 0;
    }
#else
    if (!ksu_su_compat_enabled) {
        return 0;
    }
#endif

    if (!ksu_is_allow_uid_for_current(ksu_get_uid_t(current_uid())))
        return 0;

    ksu_strncpy_from_user_nofault(path, *filename_user, sizeof(path));

    if (unlikely(!memcmp(path, su_path, sizeof(su_path)))) {
        old_cred = override_creds(ksu_cred);
        if (is_ksud_exists()) {
            pr_info("ksu_handle_faccessat su->sh!\n");
            *filename_user = sh_user_path();
        } else {
            pr_info("no ksud found, don't process faccessat for su!");
        }

        revert_creds(old_cred);
    }

    return 0;
}

int ksu_handle_stat_4_19(int *dfd, const char __user **filename_user, int *flags)
{
    const struct cred *old_cred;
    char path[sizeof(su_path) + 1] = { 0 };

#ifndef CONFIG_KSU_TRACEPOINT_HOOK
    if (ksu_is_current_proc_unprivillege()) {
        return 0;
    }
#endif

#ifdef KSU_COMPAT_USE_STATIC_KEY
    // Yep, maybe someusers love turn off sucompat <- idk how they managed to keep using it
    // But for mostly users, sucompat is enabled, so unlikely here
    if (!static_branch_unlikely(&ksu_su_compat_enabled)) {
        return 0;
    }
#else
    if (!ksu_su_compat_enabled) {
        return 0;
    }
#endif

    if (unlikely(!filename_user)) {
        return 0;
    }

    if (!ksu_is_allow_uid_for_current(ksu_get_uid_t(current_uid())))
        return 0;

    ksu_strncpy_from_user_nofault(path, *filename_user, sizeof(path));

    if (unlikely(!memcmp(path, su_path, sizeof(su_path)))) {
        old_cred = override_creds(ksu_cred);
        if (is_ksud_exists()) {
            pr_info("ksu_handle_stat su->sh!\n");
            *filename_user = sh_user_path();
        } else {
            pr_info("no ksud found, don't process stat for su!");
        }

        revert_creds(old_cred);
    }

    return 0;
}
