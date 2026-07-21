# Tools

项目随行工具脚本。`.py` 均为**编辑器内 python**（经 UnrealAgentMCP `console_command` 执行 `py "<路径>"`，或编辑器 Python 控制台直跑）；脚本顶部常量含目标资产路径，换场景/包时改常量复用。产物与进度 marker 写 `Saved/*.json`。

## Minimap/ — 小地图烘焙管线（换大场景三步复现）

| 脚本 | 作用 |
|---|---|
| `bake_minimap.py` | 场景包围盒扫描（滤天空球）→ 正交 **BaseColor** 俯拍（FinalColor 会烘进光照=废图）→ 导出 PNG → 导入纹理 + 摆 `AMAMapDefinition` |
| `minimap_outline.py` | **本机 python**（非编辑器）：障碍掩码描边（中值地面色 + RGB 距离 + 闭/开运算），Sobel 阈值法已证伪勿回头 |
| `reimport_minimap.py` | 描边产物覆盖导入同名纹理资产（widget 零改动生效） |

## ContentOps/ — 外部资产包运维

| 脚本 | 作用 |
|---|---|
| `migrate_pack_to_external.py` | 整包迁入 `/Game/External/<包名>`：`rename_directory`（失败退逐资产）→ redirector 手工修复（引擎 `fixup_referencers` 无 python 绑定：重存引用者→删跳转）→ 盘点。**先按原 /Game 路径拷入再迁**（uasset 引用是绝对包路径）；GB 级包注意 OOM（整目录改名全量载内存，迁完重启编辑器泄压）；迁后清 `SK_*` 旧路径脏副本 |

## LevelOps/ — 关卡批量运维

| 脚本 | 作用 |
|---|---|
| `scan_outliner.py` | Outliner 现状盘点：类分布 + 命名样本 + 既有文件夹（定分类规则的依据） |
| `organize_outliner.py` | 按「类映射 + 标签关键词」批量 `set_folder_path`，未命中落 Misc 并统计词元 |
| `organize_outliner_pass2.py` | 二轮：按包私有词表（从一轮 Misc 词元反推）细分 |
| `place_playerstarts.py` | 沿 Roads 文件夹路面撒 PlayerStart：包围盒顶面 + 悬空 150uu（python 拆不开 HitResult，免线迹），最小间距贪心散布；顺带核对 WorldSettings GameModeOverride |

## 根目录

- `SetupApiKey.bat` — 发行包配套：一次输入写入用户环境变量 `MOONSHOT_API_KEY`（批处理必须 CRLF）
