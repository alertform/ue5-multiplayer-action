# -*- coding: utf-8 -*-
"""LAKETOWN 迁入 External:整目录改名(失败则逐资产)+ redirector 手工修复 + 存盘。
沿用 2026-07-21 外部资产迁移管线(引擎 fixup_referencers 无 python 绑定,手工重存引用者)。"""
import json
import traceback

import unreal

LOG = 'F:/MultiPlayerAction/Saved/move_laketown.json'
state = {}
EAL = unreal.EditorAssetLibrary
ar = unreal.AssetRegistryHelpers.get_asset_registry()
SRC = '/Game/LAKETOWN'
DST = '/Game/External/LAKETOWN'


def flush():
    with open(LOG, 'w', encoding='utf-8') as f:
        json.dump(state, f, ensure_ascii=False, indent=1)


try:
    assets = [str(a) for a in EAL.list_assets(SRC, recursive=True)]
    state['src_assets'] = len(assets)
    flush()
    assert assets, 'LAKETOWN not in registry - editor scan incomplete?'

    ok = False
    try:
        ok = EAL.rename_directory(SRC, DST)
    except Exception as e:
        state['rename_dir_error'] = str(e)[:300]
    state['rename_directory'] = ok
    flush()

    if not ok:
        moved, fails = 0, []
        for path in assets:
            pkg = path.split('.')[0]
            dst = pkg.replace(SRC, DST)
            try:
                if EAL.rename_asset(pkg, dst):
                    moved += 1
                else:
                    fails.append({'asset': pkg, 'err': 'rename-false'})
            except Exception as e:
                fails.append({'asset': pkg, 'err': str(e)[:150]})
        state['per_asset_moved'] = moved
        state['per_asset_fails'] = fails
        flush()

    # redirector 修复:重存引用者(载入即解析),再删无人引用的 redirector
    flt = unreal.ARFilter(
        package_paths=['/Game'], recursive_paths=True,
        class_paths=[unreal.TopLevelAssetPath('/Script/CoreUObject', 'ObjectRedirector')])
    redirs = list(ar.get_assets(flt))
    state['redirectors'] = len(redirs)
    flush()
    resave_fail = []
    for rd in redirs:
        for ref in (ar.get_referencers(rd.package_name, unreal.AssetRegistryDependencyOptions()) or []):
            try:
                pkg = unreal.load_package(str(ref))
                if not (pkg and unreal.EditorLoadingAndSavingUtils.save_packages([pkg], False)):
                    resave_fail.append(str(ref))
            except Exception as e:
                resave_fail.append('%s :: %s' % (ref, str(e)[:120]))
    ar.scan_paths_synchronous(['/Game'], force_rescan=True)
    deleted, kept = [], []
    for rd in redirs:
        pkg_name = str(rd.package_name)
        left = [str(r) for r in (ar.get_referencers(rd.package_name,
                unreal.AssetRegistryDependencyOptions()) or [])]
        if left:
            kept.append({'rd': pkg_name, 'left': left[:5]})
        else:
            try:
                EAL.delete_asset(pkg_name)
                deleted.append(pkg_name)
            except Exception as e:
                kept.append({'rd': pkg_name, 'err': str(e)[:120]})
    state['redirector_deleted'] = len(deleted)
    state['redirector_kept'] = kept
    state['resave_fail'] = resave_fail
    flush()

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    # 盘点:剩余源目录资产 + 目的地图列表(告诉用户开哪张)
    state['src_left'] = len(EAL.list_assets(SRC, recursive=True)) if EAL.does_directory_exist(SRC) else 0
    maps_flt = unreal.ARFilter(
        package_paths=[DST], recursive_paths=True,
        class_paths=[unreal.TopLevelAssetPath('/Script/Engine', 'World')])
    state['maps'] = [str(a.package_name) for a in ar.get_assets(maps_flt)]
    state['done'] = True
except Exception:
    state['fatal'] = traceback.format_exc()[-800:]
flush()
unreal.log('MOVE_LAKETOWN DONE')
