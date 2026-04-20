"""
pack_meshes_hand.py
Packs skeleton.json + all *.obj files from the Meshes_hand/ folder
into a single binary archive for embedding as a Win32 RCDATA resource.

Pack format: identical to preview_meshes.pack (magic "PMSH", version 1).
No Diffuse.png is included — hand meshes use a flat grey fallback colour.

Run from the project root:
  python BuildEvent/pack_meshes_hand.py
Output:
  data/Meshes_hand/preview_meshes_hand.pack
"""

import struct
import os
import sys

MESHES_DIR = "data/Meshes_hand"
OUTPUT     = "data/Meshes_hand/preview_meshes_hand.pack"

def main():
    if not os.path.isdir(MESHES_DIR):
        print(f"WARNING: meshes folder not found: {MESHES_DIR}, skipping pack.")
        sys.exit(0)

    files = sorted(
        f for f in os.listdir(MESHES_DIR)
        if f.endswith(".obj") or f == "skeleton.json"
    )

    if not files:
        print("ERROR: no .obj / skeleton.json files found.")
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
