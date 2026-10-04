import { exec } from 'kernelsu'

// 统一命令执行出口：任何失败（含非 KernelSU 环境打开）都归一为 {ok:false}
export async function run(cmd) {
  try {
    const { errno, stdout, stderr } = await exec(cmd)
    return { ok: errno === 0, errno, stdout: stdout || '', stderr: stderr || '' }
  } catch (e) {
    return { ok: false, errno: -1, stdout: '', stderr: String(e) }
  }
}
