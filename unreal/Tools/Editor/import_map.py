"""/Game/UI/Map: the world map's satellite pictures (M2; docs/regions.md,
"The satellite map").

Imports every unreal/Content-src/map/<region id>.png (the game's map bake,
AcMapBake.h, writes them) as /Game/UI/Map/T_AcMap_<id> (the id's dashes as
underscores): sRGB, with its alpha (the ground out of play), mips for the
zoomed-out world, never streamed (the map shows them at once).

Rerunnable. Run it through the bake (uv run unreal/Tools/bake_map.py) or on
its own:
  uv run unreal/Tools/hidden_run.py --out unreal/server/build/map-import --python unreal/Tools/Editor/import_map.py
"""
import glob
import os

import unreal

DIR = "/Game/UI/Map"
SRC = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Content-src", "map"))

assets = unreal.EditorAssetLibrary
pngs = sorted(glob.glob(os.path.join(SRC, "*.png")))
if not pngs:
    unreal.log_error("import_map: no pictures in %s (bake first)" % SRC)
for png in pngs:
    region = os.path.splitext(os.path.basename(png))[0]
    name = "T_AcMap_" + region.replace("-", "_")
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", png)
    task.set_editor_property("destination_path", DIR)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    path = DIR + "/" + name
    tex = unreal.load_asset(path)
    if tex is None:
        unreal.log_error("import_map: %s did not import" % png)
        continue
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("never_stream", True)
    assets.save_asset(path, only_if_is_dirty=False)
    unreal.log("import_map: %s -> %s (%d x %d)" % (os.path.basename(png), path, tex.blueprint_get_size_x(), tex.blueprint_get_size_y()))
