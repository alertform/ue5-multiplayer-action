# -*- coding: utf-8 -*-
"""LAKETOWN 出生点布置:沿 Roads 文件夹的路面 actor 下探线迹找地面,贪心散布 7 个新
PlayerStart(加上原有 1 个共 8),间距 >= 4000uu;核对/设置地图 GameModeOverride。"""
import json
import random
import traceback

import unreal

LOG = 'F:/MultiPlayerAction/Saved/place_playerstarts.json'
state = {}
TARGET = '/Game/External/LAKETOWN/MAPS/LAKETOWN'
WANTED = 7
MIN_SPACING = 4000.0

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def flush():
    with open(LOG, 'w', encoding='utf-8') as f:
        json.dump(state, f, ensure_ascii=False, indent=1)


try:
    world = ues.get_editor_world()
    if world.get_package().get_name() != TARGET:
        les.load_level(TARGET)
        world = ues.get_editor_world()

    # GameMode 归属核对
    ws = world.get_world_settings()
    override = ws.get_editor_property('default_game_mode')
    state['gamemode_override_before'] = str(override)
    gm = unreal.load_class(None, '/Script/MultiPlayerAction.MultiPlayerActionGameMode')
    if override != gm:
        ws.set_editor_property('default_game_mode', gm)
        state['gamemode_override_set'] = True

    all_actors = actors_sub.get_all_level_actors()
    existing = [a for a in all_actors if isinstance(a, unreal.PlayerStart)]
    roads = [a for a in all_actors if str(a.get_folder_path()) == 'Environment/Roads']
    state['existing_starts'] = len(existing)
    state['road_actors'] = len(roads)
    flush()

    # 候选点:路面 actor 包围盒中心 + 长轴四分位点
    candidates = []
    for r in roads:
        origin, ext = r.get_actor_bounds(False)
        pts = [(origin.x, origin.y)]
        if ext.x > ext.y and ext.x > 1500:
            pts += [(origin.x - ext.x * 0.5, origin.y), (origin.x + ext.x * 0.5, origin.y)]
        elif ext.y > 1500:
            pts += [(origin.x, origin.y - ext.y * 0.5), (origin.x, origin.y + ext.y * 0.5)]
        for x, y in pts:
            candidates.append((x, y, origin.z, ext.z))
    random.seed(42)
    random.shuffle(candidates)
    state['candidates'] = len(candidates)
    flush()

    # 免线迹:路面 actor 是薄板,包围盒顶面≈路面;出生点悬空 150uu,pawn 落地自稳。
    chosen = []
    anchors = [a.get_actor_location() for a in existing]
    for x, y, z_ref, z_ext in candidates:
        if len(chosen) >= WANTED:
            break
        p = unreal.Vector(x, y, z_ref + z_ext + 150.0)
        too_close = False
        for q in anchors + chosen:
            if (unreal.Vector(p.x, p.y, 0) - unreal.Vector(q.x, q.y, 0)).length() < MIN_SPACING:
                too_close = True
                break
        if not too_close:
            chosen.append(p)

    spawned = []
    for i, p in enumerate(chosen):
        ps = actors_sub.spawn_actor_from_class(unreal.PlayerStart, p, unreal.Rotator())
        ps.set_actor_label('PlayerStart_%d' % (i + 2))
        ps.set_folder_path(unreal.Name('Gameplay'))
        spawned.append([round(p.x), round(p.y), round(p.z)])
    for a in existing:
        a.set_folder_path(unreal.Name('Gameplay'))

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    state['spawned'] = spawned
    state['total_starts'] = len(existing) + len(spawned)
    state['done'] = True
except Exception:
    state['fatal'] = traceback.format_exc()[-800:]
flush()
unreal.log('PLACE_PLAYERSTARTS DONE')
