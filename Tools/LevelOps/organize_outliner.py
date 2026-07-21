# -*- coding: utf-8 -*-
"""LAKETOWN Outliner 整理:按类 + 命名关键词归档文件夹,未命中词元落 Misc 并统计。"""
import json
import re
from collections import Counter

import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

TARGET = '/Game/External/LAKETOWN/MAPS/LAKETOWN'
world = ues.get_editor_world()
if world.get_package().get_name() != TARGET:
    les.load_level(TARGET)

# 类 → 文件夹(优先级最高)
CLASS_MAP = {
    'DirectionalLight': 'Lighting/Sky',
    'SkyLight': 'Lighting/Sky',
    'RectLight': 'Lighting/Local',
    'PointLight': 'Lighting/Local',
    'SpotLight': 'Lighting/Local',
    'BP_NIGHT_LIGHT4_C': 'Lighting/Local',
    'SkyAtmosphere': 'Atmosphere',
    'ExponentialHeightFog': 'Atmosphere',
    'VolumetricCloud': 'Atmosphere',
    'PostProcessVolume': 'Rendering',
    'LightmassImportanceVolume': 'Rendering',
    'SphereReflectionCapture': 'Rendering',
    'BoxReflectionCapture': 'Rendering',
    'DecalActor': 'Decals',
    'GroupActor': 'Groups',
    'BP_Spline_C': 'Splines',
    'Landscape': 'Landscape',
    'LandscapeStreamingProxy': 'Landscape',
    'InstancedFoliageActor': 'Environment/Foliage',
    'PlayerStart': 'Gameplay',
    'CameraActor': 'Gameplay',
    'WindDirectionalSource': 'FX',
    'SkeletalMeshActor': 'FX',
}

# StaticMeshActor 标签关键词 → 文件夹(顺序即优先级)
KEYWORD_MAP = [
    (('SKY_DOME', 'SKYDOME', 'CLOUD'), 'Atmosphere'),
    (('BILLBOARD', 'SIGN', 'NEON', 'HOLOGRAM', 'HOLO', 'SCREEN', 'AD_'), 'Environment/Signage'),
    (('BUILDING', 'BLD', 'TOWER', 'SKYSCRAPER', 'HOUSE', 'FACADE'), 'Environment/Buildings'),
    (('ROAD', 'STREET', 'SIDEWALK', 'BRIDGE', 'HIGHWAY', 'CURB', 'RAIL', 'TUNNEL'), 'Environment/Roads'),
    (('TREE', 'PLANT', 'BUSH', 'GRASS', 'FLOWER', 'IVY', 'FOLIAGE', 'LEAF'), 'Environment/Foliage'),
    (('ROCK', 'CLIFF', 'STONE', 'MOUNTAIN'), 'Environment/Rocks'),
    (('WATER', 'LAKE', 'RIVER', 'OCEAN', 'SEA'), 'Environment/Water'),
    (('VEHICLE', 'CAR', 'SHIP', 'BOAT', 'TRAIN', 'DRONE'), 'Environment/Vehicles'),
    (('LAMP', 'POLE', 'FENCE', 'BARRIER', 'BENCH', 'PROP', 'CRATE', 'CONTAINER',
      'PIPE', 'WIRE', 'CABLE', 'ANTENNA', 'VENT', 'DOOR', 'WINDOW'), 'Environment/Props'),
]

counts = Counter()
misc_tokens = Counter()
for a in actors_sub.get_all_level_actors():
    cls = a.get_class().get_name()
    label = a.get_actor_label().upper()
    folder = CLASS_MAP.get(cls)
    if folder is None and cls == 'StaticMeshActor':
        for keys, dest in KEYWORD_MAP:
            if any(k in label for k in keys):
                folder = dest
                break
        if folder is None:
            folder = 'Environment/Misc'
            for tok in re.split(r'[_\d]+', label):
                if len(tok) >= 3 and tok != 'SM':
                    misc_tokens[tok] += 1
    if folder is None:
        folder = 'Misc'
    a.set_folder_path(unreal.Name(folder))
    counts[folder] += 1

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
out = {'folders': dict(counts.most_common()), 'misc_top_tokens': dict(misc_tokens.most_common(30))}
with open('F:/MultiPlayerAction/Saved/laketown_outliner_result.json', 'w', encoding='utf-8') as f:
    json.dump(out, f, ensure_ascii=False, indent=1)
unreal.log('ORGANIZE DONE')
