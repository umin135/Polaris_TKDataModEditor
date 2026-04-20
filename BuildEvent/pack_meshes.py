"""
pack_meshes.py
Packs skeleton.json + all *.obj files + Diffuse.png from the Meshes/ folder
into a single binary archive for embedding as a Win32 RCDATA resource.

Pack format:
  magic     : 4 bytes  "PMSH"
  version   : uint32 LE  (= 1)
  file_count: uint32 LE
  for each file:
    name_len : uint32 LE
    name     : char[name_len]   (basename only, e.g. "Head.obj")
    data_len : uint32 LE
    data     : byte[data_len]

Run from the project root:
  python _references/moveset_anim/pack_meshes.py
Output:
  data/preview_meshes.pack
"""

import struct
import os
import sys

MESHES_DIR = "data/Meshes"
OUTPUT     = "data/Meshes/preview_meshes.pack"

def main():
    if not os.path.isdir(MESHES_DIR):
        print(f"WARNING: meshes folder not found: {MESHES_DIR}, skipping pack.")
        sys.exit(0)

    files = sorted(
        f for f in os.listdir(MESHES_DIR)
        if f.endswith(".obj") or f == "skeleton.json" or f == "Diffuse.png"
    )

    if not files:
        print("ERROR: no .obj / skeleton.json / Diffuse.png files found.")
        sys.exit(1)

    os.makedirs(os.path.dirname(OUTPUT) or ".", exist_ok=True)

    with open(OUTPUT, "wb") as out:
        out.write(b"PMSH")                       # magic
        out.write(struct.pack("<I", 1))           # version
        out.write(struct.pack("<I", len(files)))  # file_count

        for fname in files:
            fpath = os.path.join(MESHES_DIR, fname)
            with open(fpath, "rb") as f:
                data = f.read()
            name = fname.encode("utf-8")
            out.write(struct.pack("<I", len(name)))
            out.write(name)
            out.write(struct.pack("<I", len(data)))
            out.write(data)
            print(f"  + {fname:30s}  {len(data):>8,} bytes")

    total = os.path.getsize(OUTPUT)
    print(f"\nPacked {len(files)} files -> {OUTPUT}  ({total:,} bytes total)")

if __name__ == "__main__":
    main()
