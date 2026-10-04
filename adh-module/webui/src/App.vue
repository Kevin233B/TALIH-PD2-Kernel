<script setup>
import { computed, onMounted, ref } from 'vue'
import {
  MiuixTopAppBar,
  MiuixScrollArea,
  MiuixSearchBar,
  MiuixSmallTitle,
  MiuixCard,
  MiuixSwitchPreference,
  MiuixArrowPreference,
  MiuixCheckboxPreference,
  MiuixButton,
  MiuixText,
  MiuixDialog,
  MiuixSnackbarHost,
  showSnackbar
} from 'miuix-vue'
import { run } from './ksu.js'

const MOD = '/data/adb/modules/android_data_hide'

const loading = ref(true)
const loadErr = ref('')
const kernel = ref(null)
const apps = ref([])
const query = ref('')
const searchExpanded = ref(false)
const saving = ref(false)

// 脏检查快照（保存成功后刷新）
const initial = ref('')

// 目标编辑对话框工作态
const dialogPkg = ref(null)
const dialogSel = ref({})
const dialogHide = ref(false)

onMounted(load)

async function load() {
  loading.value = true
  loadErr.value = ''
  const r = await run(`sh ${MOD}/webui.sh status`)
  loading.value = false
  if (!r.ok) {
    loadErr.value = r.stderr || r.stdout || `加载失败（errno ${r.errno}）`
    return
  }
  try {
    const st = JSON.parse(r.stdout)
    kernel.value = st.kernel
    apps.value = (st.apps || []).map(a => ({ ...a, visibleTo: a.visibleTo || [] }))
    initial.value = JSON.stringify(snapshot())
  } catch (e) {
    loadErr.value = '状态解析失败：' + e
  }
}

function snapshot() {
  return apps.value
    .filter(a => a.hidden)
    .map(a => ({ pkg: a.pkg, v: [...a.visibleTo].sort() }))
}

const dirty = computed(() => JSON.stringify(snapshot()) !== initial.value)

const hiddenApps = computed(() => filterByQuery(apps.value.filter(a => a.hidden)))
const restUserApps = computed(() => filterByQuery(apps.value.filter(a => !a.hidden && !a.sys)))
const restSysApps = computed(() => filterByQuery(apps.value.filter(a => !a.hidden && a.sys)))

function filterByQuery(list) {
  const q = query.value.trim().toLowerCase()
  if (!q) return list
  return list.filter(a => a.pkg.toLowerCase().includes(q))
}

function targetByPkg(pkg) {
  return apps.value.find(a => a.pkg === pkg)
}

// 可见授权候选：非系统应用（系统应用天然可见）、除目标自身
const dialogCandidates = computed(() => {
  if (!dialogPkg.value) return []
  return apps.value.filter(a => !a.sys && a.pkg !== dialogPkg.value)
})

function openDialog(pkg) {
  const t = targetByPkg(pkg)
  if (!t) return
  dialogPkg.value = pkg
  dialogHide.value = t.hidden
  const sel = {}
  for (const a of apps.value) {
    if (!a.sys && a.pkg !== pkg) sel[a.pkg] = t.visibleTo.includes(a.pkg)
  }
  dialogSel.value = sel
}

// 任何关闭路径（“完成”按钮或背景点击）都把工作态写回
function applyDialog() {
  const pkg = dialogPkg.value
  if (!pkg) return
  const t = targetByPkg(pkg)
  if (t) {
    t.hidden = dialogHide.value
    t.visibleTo = Object.keys(dialogSel.value).filter(p => dialogSel.value[p])
  }
}

const dialogOpen = computed({
  get: () => !!dialogPkg.value,
  set: v => {
    if (!v) applyDialog()
    dialogPkg.value = null
  }
})

async function save() {
  saving.value = true
  const hide = apps.value.filter(a => a.hidden).map(a => a.pkg).join(',')
  const vis = apps.value
    .filter(a => a.hidden && a.visibleTo.length)
    .map(a => `${a.pkg}:${a.visibleTo.join('+')}`)
    .join(',')
  const r = await run(`sh ${MOD}/webui.sh save '${hide}' '${vis}'`)
  saving.value = false
  if (r.ok && r.stdout.includes('OK')) {
    showSnackbar({ message: '已保存并发布' })
    await load()
  } else {
    showSnackbar({
      message: '保存失败：' + (r.stderr || r.stdout || `errno ${r.errno}`),
      duration: 'long'
    })
  }
}

function kernelSummary() {
  if (!kernel.value) return null
  if (!kernel.value.enabled) {
    return `未启用（名单为空或未发布）—— 父目录 ${kernel.value.parents} · 规则 0 · 系统放行 ${kernel.value.sys}`
  }
  return `已启用 —— 父目录 ${kernel.value.parents} · 封堵 ${kernel.value.rules} · 系统放行 ${kernel.value.sys}`
}
</script>

<template>
  <div class="app">
    <MiuixScrollArea class="app__body">
      <MiuixTopAppBar title="应用存在性封堵">
        <template #actions>
          <MiuixButton type="primary" :disabled="!dirty || saving" @click="save">
            {{ saving ? '发布中…' : '保存' }}
          </MiuixButton>
        </template>
      </MiuixTopAppBar>

      <MiuixSearchBar
        v-model="query"
        v-model:expanded="searchExpanded"
        label="搜索应用"
        class="sect-mb12"
      />

      <MiuixText v-if="loading" type="body2" class="pad-hint">加载中…</MiuixText>
      <div v-else-if="loadErr" class="pad-hint">
        <MiuixText type="body2">{{ loadErr }}</MiuixText>
        <MiuixButton type="primary" class="retry-btn" @click="load">重试</MiuixButton>
      </div>

      <template v-else>
        <MiuixSmallTitle text="内核状态" />
        <MiuixCard class="sect-mb12">
          <MiuixText v-if="!kernel" type="body2" class="warn-text">
            内核补丁未生效（CONFIG_ANDROID_DATA_HIDE / sysfs 缺失）。本页仍可编辑保存配置，但不会拦截任何探测。
          </MiuixText>
          <template v-else>
            <MiuixText type="body2">{{ kernelSummary() }}</MiuixText>
            <MiuixText type="footnote2" class="dim-text">
              属主自身、root/system/shell/media 与预装系统应用始终可见；被授权应用按目标单独放行。
            </MiuixText>
          </template>
        </MiuixCard>

        <MiuixSmallTitle :text="`已封堵（${hiddenApps.length}）`" />
        <MiuixCard class="sect-mb12">
          <template v-if="hiddenApps.length">
            <MiuixArrowPreference
              v-for="a in hiddenApps"
              :key="a.pkg"
              :title="a.pkg"
              :summary="`uid ${a.uid}${a.sys ? ' · 系统' : ''} · 对 ${a.visibleTo.length} 个应用可见`"
              @click="openDialog(a.pkg)"
            />
          </template>
          <MiuixText v-else type="body2" class="empty-text">无——所有应用互相可见（stock 行为）</MiuixText>
        </MiuixCard>

        <MiuixSmallTitle :text="`用户应用（${restUserApps.length}）`" />
        <MiuixCard class="sect-mb12">
          <template v-if="restUserApps.length">
            <MiuixSwitchPreference
              v-for="a in restUserApps"
              :key="a.pkg"
              v-model="a.hidden"
              :title="a.pkg"
              :summary="`uid ${a.uid}`"
            />
          </template>
          <MiuixText v-else type="body2" class="empty-text">无</MiuixText>
        </MiuixCard>

        <MiuixSmallTitle :text="`系统应用（${restSysApps.length}）`" />
        <MiuixCard class="sect-mb12">
          <MiuixSwitchPreference
            v-for="a in restSysApps"
            :key="a.pkg"
            v-model="a.hidden"
            :title="a.pkg"
            :summary="`uid ${a.uid} · 系统`"
          />
        </MiuixCard>

        <MiuixText type="footnote2" class="dim-text pad-hint">
          切换即加入/移出封堵名单；点击“已封堵”条目可编辑单个目标的可见授权。
          保存后立即发布，装卸应用 ≤10 秒自动收敛。
        </MiuixText>
      </template>
    </MiuixScrollArea>
  </div>

  <MiuixDialog
    v-model="dialogOpen"
    :title="dialogPkg || ''"
    summary="封堵开关与可见授权（系统应用天然可见，无需授权）"
  >
    <template #default>
      <MiuixSwitchPreference
        v-model="dialogHide"
        title="封堵此应用"
        summary="对非属主普通应用表现为未安装"
      />
      <MiuixText type="body2" class="dialog-sub">可见授权（普通应用）</MiuixText>
      <div class="dialog-list">
        <MiuixCheckboxPreference
          v-for="c in dialogCandidates"
          :key="c.pkg"
          v-model="dialogSel[c.pkg]"
          :title="c.pkg"
          :summary="`uid ${c.uid}`"
        />
      </div>
      <MiuixButton type="primary" class="dialog-ok" @click="dialogOpen = false">完成</MiuixButton>
    </template>
  </MiuixDialog>

  <MiuixSnackbarHost />
</template>

<style>
html,
body {
  margin: 0;
  height: 100%;
}

body {
  font-family: 'MiSans VF', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  background: var(--m-color-background);
  color: var(--m-color-on-background);
}

.app {
  display: flex;
  flex-direction: column;
  height: 100dvh;
  min-height: 0;
  background: var(--m-color-surface);
}

.app__body {
  flex: 1;
  min-height: 0;
  --m-scroll-area-inset-top: 52px;
}

.sect-mb12 {
  margin: 0 12px 12px;
}

.pad-hint {
  padding: 12px;
  display: block;
}

.warn-text {
  padding: 16px;
  display: block;
}

.dim-text {
  opacity: 0.72;
  display: block;
  padding-bottom: 8px;
}

.empty-text {
  padding: 16px;
  display: block;
}

.retry-btn {
  margin-top: 12px;
}

.dialog-sub {
  display: block;
  margin: 12px 0 4px;
}

.dialog-list {
  max-height: 46dvh;
  overflow-y: auto;
  margin: 0 -20px;
  padding: 0 20px;
}

.dialog-ok {
  width: 100%;
  margin-top: 12px;
}
</style>
