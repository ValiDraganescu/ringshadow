"""The Brood's scale lineups (docs/creatures.md; N1's pictures): copies of the model row level with only the Prospector and
the bugs, so a hidden picture run frames them side by side on the floor under the row's sun and sky.

    uv run unreal/Tools/hidden_run.py --out unreal/server/build/n1/lineup --python unreal/Tools/Editor/make_brood_lineup.py
    uv run unreal/Tools/hidden_run.py --out unreal/server/build/n1/shots -- /Game/Maps/BroodLineup -AcModelFocus=row \
        -AcNoSave -AcNoAI -AcPaused -AcShot={out}/lineup.png

/Game/Maps/BroodLineup: the Prospector, a Shardling, a Gloamwing (perched: its rest pose), a Lodeback and the Matriarch.
/Game/Maps/BroodLineupSmall: the same without the Matriarch, so the small ones read beside the Prospector.
"""
import unreal

LINEUPS = {
    "/Game/Maps/BroodLineup": "prospector_blue,shardling_blue,gloamwing_blue,lodeback_blue,matriarch_blue",
    "/Game/Maps/BroodLineupSmall": "prospector_blue,shardling_blue,gloamwing_blue,lodeback_blue",
}
SRC = "/Game/Maps/ModelRow"

for dst, models in LINEUPS.items():
    if not unreal.EditorAssetLibrary.does_asset_exist(dst):
        unreal.EditorAssetLibrary.duplicate_asset(SRC, dst)
    unreal.EditorLoadingAndSavingUtils.load_map(dst)
    rows = [a for a in unreal.EditorLevelLibrary.get_all_level_actors() if a.get_class().get_name() == "AcModelRow"]
    if len(rows) != 1:
        raise RuntimeError("%s: %d model rows, want 1" % (dst, len(rows)))
    row = rows[0]
    row.set_editor_property("models", models)
    row.set_editor_property("palette_row", False)
    row.set_editor_property("include_red", False)
    row.set_editor_property("gap", 120.0)
    unreal.EditorLoadingAndSavingUtils.save_current_level()
    unreal.log("[brood] lineup %s: %s" % (dst, models))
