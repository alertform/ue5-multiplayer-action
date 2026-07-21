# -*- coding: utf-8 -*-
"""竞技场小地图烘焙:正交俯拍(Pitch=-90/Yaw=0,屏上=+X 屏右=+Y,与 WorldToMapUV 约定一致)
→ RT 导出 PNG → 导入为纹理资产 → 摆 AMAMapDefinition 标定范围并挂纹理 → 存盘。"""
import json
import traceback

import unreal

OUT = 'F:/MultiPlayerAction/Saved/bake_minimap.json'
state = {}


def flush():
    with open(OUT, 'w', encoding='utf-8') as f:
        json.dump(state, f, ensure_ascii=False, indent=1)


try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    les.load_level('/Game/Maps/ThirdPersonMap')
    world = ues.get_editor_world()
    state['map'] = world.get_package().get_name()
    flush()

    # 场景包围盒(静态网格,滤掉天空球等超大件)
    mn = [1e12, 1e12, 1e12]
    mx = [-1e12, -1e12, -1e12]
    used = 0
    for a in actors.get_all_level_actors():
        if not isinstance(a, unreal.StaticMeshActor):
            continue
        if 'sky' in a.get_actor_label().lower():
            continue
        origin, ext = a.get_actor_bounds(False)
        if max(ext.x, ext.y) > 50000:
            continue
        used += 1
        for i, (o, e) in enumerate(((origin.x, ext.x), (origin.y, ext.y), (origin.z, ext.z))):
            mn[i] = min(mn[i], o - e)
            mx[i] = max(mx[i], o + e)
    assert used > 0, 'no static meshes found'
    cx, cy = (mn[0] + mx[0]) / 2, (mn[1] + mx[1]) / 2
    span = max(mx[0] - mn[0], mx[1] - mn[1]) * 1.05
    state['bounds'] = {'center': [cx, cy], 'span': span, 'actors_used': used}
    flush()

    # 正交捕捉
    rt = unreal.RenderingLibrary.create_render_target2d(
        world, 2048, 2048, unreal.TextureRenderTargetFormat.RTF_RGBA8,
        unreal.LinearColor.BLACK, False)
    cap = actors.spawn_actor_from_class(
        unreal.SceneCapture2D,
        unreal.Vector(cx, cy, mx[2] + 3000.0),
        unreal.Rotator(roll=0.0, pitch=-90.0, yaw=0.0))
    cc = cap.capture_component2d
    cc.set_editor_property('projection_type', unreal.CameraProjectionMode.ORTHOGRAPHIC)
    cc.set_editor_property('ortho_width', span)
    cc.set_editor_property('texture_target', rt)
    cc.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_BASE_COLOR)
    cc.set_editor_property('capture_every_frame', False)
    cc.capture_scene()
    unreal.RenderingLibrary.export_render_target(
        world, rt, 'F:/MultiPlayerAction/Saved/', 'T_Minimap_ThirdPersonMap.png')
    actors.destroy_actor(cap)
    state['captured'] = True
    flush()

    # 导入为资产
    task = unreal.AssetImportTask()
    task.filename = 'F:/MultiPlayerAction/Saved/T_Minimap_ThirdPersonMap.png'
    task.destination_path = '/Game/UI/Minimap'
    task.automated = True
    task.save = True
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.EditorAssetLibrary.load_asset('/Game/UI/Minimap/T_Minimap_ThirdPersonMap')
    assert tex, 'texture import failed'
    state['texture'] = tex.get_path_name()
    flush()

    # 地图定义 actor(已存在则复用)
    md = None
    for a in actors.get_all_level_actors():
        if isinstance(a, unreal.MAMapDefinition):
            md = a
            break
    if not md:
        md = actors.spawn_actor_from_class(
            unreal.MAMapDefinition,
            unreal.Vector(cx, cy, (mn[2] + mx[2]) / 2), unreal.Rotator())
    md.set_actor_location(unreal.Vector(cx, cy, (mn[2] + mx[2]) / 2), False, False)
    box = md.get_editor_property('bounds')
    box.set_editor_property('box_extent', unreal.Vector(span / 2, span / 2, 2000.0))
    md.set_editor_property('minimap_texture', tex)
    state['map_def'] = md.get_path_name()
    flush()

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    state['done'] = True
except Exception:
    state['error'] = traceback.format_exc()[-900:]
flush()
unreal.log('BAKE_MINIMAP DONE')
