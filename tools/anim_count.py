# list_animations.py
from pygltflib import GLTF2
import sys
import json

if len(sys.argv) < 2:
    print(f"Usage: {sys.argv[0]} <file.gltf|file.glb>")
    sys.exit(1)

filename = sys.argv[1]

gltf = GLTF2().load(filename)

if not gltf.animations:
    print("No animations found.")
    sys.exit(0)

result = []
for i, anim in enumerate(gltf.animations):
    name = anim.name or f"Anim_{i}"
    result.append({"index": i, "name": name})

# 単純表示
for a in result:
    print(f"[{a['index']}] {a['name']}")

# そのままJSON雛形にしたい場合
# print(json.dumps(result, indent=2, ensure_ascii=False))