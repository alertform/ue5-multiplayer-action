# -*- coding: utf-8 -*-
"""扫描 LAKETOWN 地图的 actor 构成:类分布 + 命名样本,为 Outliner 分类定规则。"""
import json
from collections import Counter, defaultdict

import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

TARGET = '/Game/External/LAKETOWN/MAPS/LAKETOWN'
world = ues.get_editor_world()
if world.get_package().get_name() != TARGET:
    les.load_level(TARGET)

actors = actors_sub.get_all_level_actors()
by_class = Counter()
samples = defaultdict(list)
folders = Counter()
for a in actors:
    cls = a.get_class().get_name()
    by_class[cls] += 1
    if len(samples[cls]) < 6:
        samples[cls].append(a.get_actor_label())
    folders[str(a.get_folder_path())] += 1

out = {
    'total': len(actors),
    'by_class': dict(by_class.most_common()),
    'samples': {k: v for k, v in samples.items()},
    'existing_folders': dict(folders.most_common(20)),
}
with open('F:/MultiPlayerAction/Saved/laketown_outliner_scan.json', 'w', encoding='utf-8') as f:
    json.dump(out, f, ensure_ascii=False, indent=1)
unreal.log('SCAN DONE: %d actors' % len(actors))
