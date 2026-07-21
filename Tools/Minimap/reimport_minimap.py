
import unreal
task = unreal.AssetImportTask()
task.filename = 'F:/MultiPlayerAction/Saved/import_tmp/T_Minimap_ThirdPersonMap.png'
task.destination_path = '/Game/UI/Minimap'
task.automated = True
task.save = True
task.replace_existing = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
t = unreal.EditorAssetLibrary.load_asset('/Game/UI/Minimap/T_Minimap_ThirdPersonMap')
unreal.log('REIMPORT OK: %s' % t.get_path_name())
