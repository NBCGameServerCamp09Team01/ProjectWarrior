"""W2-5: DA_StageWave_Test 생성 (wave-w2-plan-0929 6장 기준).

에디터 Output Log(Python 모드) 또는 File > Execute Python Script로 실행한다.
  - 에셋이 없으면 생성하고 저장한다.
  - 이미 있으면 덮어쓰지 않고 내용만 검증한다. (값이 다르면 에셋을 지우고 다시 실행)

구성: 웨이브 1~4 = 적 2/3/3/4, 웨이브 5 = 보스 1 + 적 2 (bBossWave)
SpawnGroup은 모두 None (스폰 포인트 그룹 None과 맞춤)
"""
import unreal

ASSET_DIR = "/Game/MyGameContents/Stage"
ASSET_NAME = "DA_StageWave_Test"
ASSET_PATH = ASSET_DIR + "/" + ASSET_NAME
ENEMY_PATH = "/Game/MyGameContents/Character/AI/BP_WarriorAICharacter.BP_WarriorAICharacter_C"

# 웨이브별 (적 수, 보스 여부) 목록
WAVES = [
    [(2, False)],
    [(3, False)],
    [(3, False)],
    [(4, False)],
    [(1, True), (2, False)],
]
BOSS_WAVE_INDEX = 4

enemy_class = unreal.load_class(None, ENEMY_PATH)
if not enemy_class:
    raise RuntimeError("Missing AI Blueprint: " + ENEMY_PATH)

assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
asset = assets.load_asset(ASSET_PATH) if assets.does_asset_exist(ASSET_PATH) else None
created = asset is None

if created:
    asset_class = unreal.load_class(None, "/Script/ProjectWarrior.WarriorStageWaveDataAsset")
    if not asset_class:
        raise RuntimeError("WarriorStageWaveDataAsset class not found. Build the project first.")
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(ASSET_NAME, ASSET_DIR, asset_class, factory)
    if not asset:
        raise RuntimeError("Could not create " + ASSET_PATH)

    waves = []
    for index, groups in enumerate(WAVES):
        entries = []
        for count, is_boss in groups:
            entry = unreal.WarriorWaveEnemySpawnData()
            entry.set_editor_property("enemy_class", enemy_class)
            entry.set_editor_property("count", count)
            entry.set_editor_property("boss_enemy", is_boss)
            entry.set_editor_property("spawn_group", "None")
            entries.append(entry)
        wave = unreal.WarriorStageWaveData()
        wave.set_editor_property("enemies", entries)
        wave.set_editor_property("rest_time_override", 0.0)
        wave.set_editor_property("boss_wave", index == BOSS_WAVE_INDEX)
        waves.append(wave)
    asset.set_editor_property("waves", waves)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save " + ASSET_PATH)

# 검증 (생성 직후 또는 기존 에셋)
waves = asset.get_editor_property("waves")
assert len(waves) == len(WAVES), "Expected %d waves, got %d" % (len(WAVES), len(waves))
for index, wave in enumerate(waves):
    assert wave.get_editor_property("boss_wave") == (index == BOSS_WAVE_INDEX), "Wave %d boss flag" % (index + 1)
    assert wave.get_editor_property("rest_time_override") == 0.0, "Wave %d rest time" % (index + 1)
    entries = wave.get_editor_property("enemies")
    assert len(entries) == len(WAVES[index]), "Wave %d entry count" % (index + 1)
    for entry, (count, is_boss) in zip(entries, WAVES[index]):
        assert entry.get_editor_property("enemy_class") == enemy_class, "Wave %d enemy class" % (index + 1)
        assert entry.get_editor_property("count") == count, "Wave %d count" % (index + 1)
        assert entry.get_editor_property("boss_enemy") == is_boss, "Wave %d boss enemy" % (index + 1)
        assert str(entry.get_editor_property("spawn_group")) == "None", "Wave %d spawn group" % (index + 1)

unreal.log("[Wave] W2_ASSET_VERIFIED: %s, waves 2/3/3/4/(1 boss + 2), boss wave 5, groups None; %s"
           % (ASSET_PATH, "created" if created else "read existing"))
