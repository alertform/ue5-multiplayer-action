# -*- coding: utf-8 -*-
"""Outliner 二轮细分:只动 Environment/Misc,按包私有词表再归档。"""
import json
from collections import Counter

import unreal

actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

KEYWORD_MAP = [
    (('LCB', 'LHB', 'RESIDENCES', 'DEPOT', 'CABIN', 'HOTEL', 'MALL', 'STATION'), 'Environment/Buildings'),
    (('TURBINE', 'FERRIS', 'WHEEL', 'POOL', 'PADDLECOURT', 'PARK', 'FIELD', 'PIER', 'DOCK'), 'Environment/Structures'),
    (('SUNBED', 'ARMCHAIR', 'CHAIR', 'TENT', 'MAST', 'BAR', 'GATE', 'SPOT', 'LIGHT',
      'CHARGE', 'POINT', 'TABLE', 'UMBRELLA', 'KIOSK'), 'Environment/Props'),
    (('YACHT', 'TRUCK', 'BUS', 'TAXI'), 'Environment/Vehicles'),
    (('LOTUS', 'REED', 'LILY'), 'Environment/Foliage'),
]

counts = Counter()
left = Counter()
for a in actors_sub.get_all_level_actors():
    if str(a.get_folder_path()) != 'Environment/Misc':
        continue
    label = a.get_actor_label().upper()
    for keys, dest in KEYWORD_MAP:
        if any(k in label for k in keys):
            a.set_folder_path(unreal.Name(dest))
            counts[dest] += 1
            break
    else:
        left[label.split('_')[1] if '_' in label else label] += 1

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
out = {'moved': dict(counts.most_common()), 'misc_left': sum(left.values()),
       'left_top': dict(left.most_common(15))}
with open('F:/MultiPlayerAction/Saved/laketown_pass2.json', 'w', encoding='utf-8') as f:
    json.dump(out, f, ensure_ascii=False, indent=1)
unreal.log('PASS2 DONE')
