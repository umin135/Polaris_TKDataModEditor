# Builds data/cinematics/data.json -- the list of level-sequence (camera cutscene) master assets
# that actually exist in the game. The editor uses it to resolve each sequence's season folder
# (polaris / polaris01 / ...) and to drop sequences that have no asset.
#
# Usage: python generate_data.py <dump root>
#   <dump root> = folder that contains Game/cinematics/... (e.g. _references/_CameraSeq/OriginalSeqs)
#   Each master asset is expected as <...>/<name>_master.json (FModel/UE export layout).
#
# Output entries are package paths relative to /Game/cinematics without extension, e.g.
#   game/polaris02/aml/throw/02/aml_throw_02_cam1p_master
# version.json is NOT touched -- bump it by hand when publishing a new data.json.
import json
import os
import sys


def main():
    if len(sys.argv) < 2:
        print(__doc__ or "usage: python generate_data.py <dump root>")
        return 2
    root = os.path.join(sys.argv[1], "Game", "cinematics")
    if not os.path.isdir(root):
        print("not found:", root)
        return 1

    paths = []
    for base in ("game", "demo", "story"):
        bdir = os.path.join(root, base)
        if not os.path.isdir(bdir):
            continue
        for cur, _, files in os.walk(bdir):
            for f in files:
                if not f.endswith("_master.json"):
                    continue
                rel = os.path.relpath(os.path.join(cur, f[:-5]), root).replace("\\", "/")
                paths.append(rel)
    paths.sort()

    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data.json")
    with open(out, "w", encoding="utf-8", newline="\n") as fp:
        fp.write('{\n  "root": "/Game/cinematics",\n  "paths": [\n')
        fp.write(",\n".join("    " + json.dumps(p) for p in paths))
        fp.write("\n  ]\n}\n")
    print(f"{len(paths)} sequences -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
