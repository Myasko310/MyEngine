"""
Generates a small self-contained rigged/animated glTF 2.0 test asset:
a 3-segment vertical "flag/tail" made of 3 stacked boxes, skinned to a
3-joint chain (root -> mid -> tip), with a looping side-to-side swing
animation baked as rotation keyframes on each joint.

Output: assets/models/character_animated.gltf + character_animated.bin

Run with: python tools/generate_test_character.py
"""
import json
import struct
import os
import math

OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets", "models")
os.makedirs(OUT_DIR, exist_ok=True)
GLTF_PATH = os.path.join(OUT_DIR, "character_animated.gltf")
BIN_PATH = os.path.join(OUT_DIR, "character_animated.bin")
BIN_FILENAME = "character_animated.bin"

# ---------------------------------------------------------------------------
# Geometry: 3 stacked unit-ish boxes forming a vertical strip, segment
# boundaries at y = 0, 1, 2, 3. Each box's vertices are weighted to the two
# joints nearest its segment so bending at joint boundaries looks smooth.
# ---------------------------------------------------------------------------

def box(y0, y1):
	"""Return 8 corner positions for a box spanning y0..y1, half-width 0.4."""
	hw = 0.4
	return [
		(-hw, y0, -hw), (hw, y0, -hw), (hw, y0, hw), (-hw, y0, hw),
		(-hw, y1, -hw), (hw, y1, -hw), (hw, y1, hw), (-hw, y1, hw),
	]

BOX_FACES = [
	(0, 1, 2), (0, 2, 3),  # bottom
	(4, 6, 5), (4, 7, 6),  # top
	(0, 4, 5), (0, 5, 1),  # -z
	(1, 5, 6), (1, 6, 2),  # +x
	(2, 6, 7), (2, 7, 3),  # +z
	(3, 7, 4), (3, 4, 0),  # -x
]

positions = []
normals = []
joints = []
weights = []
indices = []

segments = [(0.0, 1.0), (1.0, 2.0), (2.0, 3.0)]

for seg_i, (y0, y1) in enumerate(segments):
	base_index = len(positions)
	corners = box(y0, y1)

	# Skinning weights: joint seg_i controls this segment, blending toward
	# joint seg_i+1 near the top face so segments bend smoothly at seams.
	joint_a = seg_i
	joint_b = min(seg_i + 1, 2)

	for (x, y, z) in corners:
		positions.append((x, y, z))
		# Simple per-vertex normal approximation (outward from center axis).
		length = math.sqrt(x * x + z * z) or 1.0
		normals.append((x / length, 0.0, z / length))

		t = (y - y0) / (y1 - y0)  # 0 at bottom of segment, 1 at top
		w_b = t
		w_a = 1.0 - t
		joints.append((joint_a, joint_b, 0, 0))
		weights.append((w_a, w_b, 0.0, 0.0))

	for (a, b, c) in BOX_FACES:
		indices.append(base_index + a)
		indices.append(base_index + b)
		indices.append(base_index + c)

vertex_count = len(positions)
index_count = len(indices)

# ---------------------------------------------------------------------------
# Skeleton: 3 joints stacked along Y, each offset by 1 unit from its parent,
# matching the segment boundaries above (root at y=0, mid at y=1, tip at y=2).
# ---------------------------------------------------------------------------

joint_translations = [
	(0.0, 0.0, 0.0),  # root (node space, root has no parent offset)
	(0.0, 1.0, 0.0),  # mid, relative to root
	(0.0, 1.0, 0.0),  # tip, relative to mid
]

# Inverse bind matrices: inverse of each joint's world transform in bind pose.
# World Y positions: root=0, mid=1, tip=2. Inverse bind = translate(-worldY).
def inverse_bind_matrix(world_y):
	# Column-major 4x4 identity with translation -world_y in row 3 (col-major
	# translation lives in elements 12-14).
	m = [1.0, 0.0, 0.0, 0.0,
		 0.0, 1.0, 0.0, 0.0,
		 0.0, 0.0, 1.0, 0.0,
		 0.0, -world_y, 0.0, 1.0]
	return m

inverse_bind_matrices = [
	inverse_bind_matrix(0.0),
	inverse_bind_matrix(1.0),
	inverse_bind_matrix(2.0),
]

# ---------------------------------------------------------------------------
# Animation: looping swing. Each non-root joint rotates back and forth around
# Z over 2 seconds, tip swinging a bit more than mid for a floppy look.
# ---------------------------------------------------------------------------

ANIM_TIMES = [0.0, 0.5, 1.0, 1.5, 2.0]

def quat_z(angle_rad):
	half = angle_rad * 0.5
	return (0.0, 0.0, math.sin(half), math.cos(half))

mid_angles = [0.0, 0.35, 0.0, -0.35, 0.0]
tip_angles = [0.0, 0.55, 0.0, -0.55, 0.0]
root_angles = [0.0, 0.0, 0.0, 0.0, 0.0]

root_rotations = [quat_z(a) for a in root_angles]
mid_rotations = [quat_z(a) for a in mid_angles]
tip_rotations = [quat_z(a) for a in tip_angles]

# ---------------------------------------------------------------------------
# Pack everything into a single binary buffer.
# ---------------------------------------------------------------------------

buffer_bytes = bytearray()

def add_chunk(fmt_iter, pack_fmt):
	start = len(buffer_bytes)
	for item in fmt_iter:
		buffer_bytes.extend(struct.pack(pack_fmt, *item))
	return start, len(buffer_bytes) - start

def align4():
	while len(buffer_bytes) % 4 != 0:
		buffer_bytes.append(0)

# Positions (VEC3 float)
pos_offset, pos_len = add_chunk(positions, "<3f")
align4()
# Normals (VEC3 float)
norm_offset, norm_len = add_chunk(normals, "<3f")
align4()
# Joints (VEC4 unsigned short)
joints_offset, joints_len = add_chunk(joints, "<4H")
align4()
# Weights (VEC4 float)
weights_offset, weights_len = add_chunk(weights, "<4f")
align4()
# Indices (unsigned int)
indices_offset, indices_len = add_chunk([(i,) for i in indices], "<I")
align4()
# Inverse bind matrices (MAT4 float x3 joints)
ibm_offset = len(buffer_bytes)
for m in inverse_bind_matrices:
	buffer_bytes.extend(struct.pack("<16f", *m))
ibm_len = len(buffer_bytes) - ibm_offset
align4()

# Animation input (time) - shared sampler input for all 3 channels
time_offset, time_len = add_chunk([(t,) for t in ANIM_TIMES], "<f")
align4()

root_rot_offset, root_rot_len = add_chunk(root_rotations, "<4f")
align4()
mid_rot_offset, mid_rot_len = add_chunk(mid_rotations, "<4f")
align4()
tip_rot_offset, tip_rot_len = add_chunk(tip_rotations, "<4f")
align4()

total_buffer_len = len(buffer_bytes)

with open(BIN_PATH, "wb") as f:
	f.write(buffer_bytes)

# ---------------------------------------------------------------------------
# Build glTF JSON.
# ---------------------------------------------------------------------------

def bounds(data, comps):
	mins = [min(v[i] for v in data) for i in range(comps)]
	maxs = [max(v[i] for v in data) for i in range(comps)]
	return mins, maxs

pos_min, pos_max = bounds(positions, 3)

gltf = {
	"asset": {"version": "2.0", "generator": "MyEngine test asset generator"},
	"scene": 0,
	# Both the skeleton root (0) and the mesh node (3) must be listed as
	# scene roots so importers (e.g. Assimp) actually traverse the joint
	# chain - otherwise it's an orphaned subgraph and no bones get parsed.
	"scenes": [{"nodes": [0, 3]}],
	"nodes": [
		{"name": "Root", "translation": list(joint_translations[0]), "children": [1]},
		{"name": "Mid", "translation": list(joint_translations[1]), "children": [2]},
		{"name": "Tip", "translation": list(joint_translations[2])},
		{"name": "CharacterMesh", "mesh": 0, "skin": 0},
	],
	"meshes": [
		{
			"name": "CharacterMesh",
			"primitives": [
				{
					"attributes": {
						"POSITION": 0,
						"NORMAL": 1,
						"JOINTS_0": 2,
						"WEIGHTS_0": 3,
					},
					"indices": 4,
					"mode": 4,
				}
			],
		}
	],
	"skins": [
		{
			"name": "CharacterSkeleton",
			"inverseBindMatrices": 5,
			"joints": [0, 1, 2],
			"skeleton": 0,
		}
	],
	"animations": [
		{
			"name": "Swing",
			"samplers": [
				{"input": 6, "interpolation": "LINEAR", "output": 7},
				{"input": 6, "interpolation": "LINEAR", "output": 8},
				{"input": 6, "interpolation": "LINEAR", "output": 9},
			],
			"channels": [
				{"sampler": 0, "target": {"node": 0, "path": "rotation"}},
				{"sampler": 1, "target": {"node": 1, "path": "rotation"}},
				{"sampler": 2, "target": {"node": 2, "path": "rotation"}},
			],
		}
	],
	"buffers": [{"uri": BIN_FILENAME, "byteLength": total_buffer_len}],
	"bufferViews": [
		{"buffer": 0, "byteOffset": pos_offset, "byteLength": pos_len, "target": 34962},
		{"buffer": 0, "byteOffset": norm_offset, "byteLength": norm_len, "target": 34962},
		{"buffer": 0, "byteOffset": joints_offset, "byteLength": joints_len, "target": 34962},
		{"buffer": 0, "byteOffset": weights_offset, "byteLength": weights_len, "target": 34962},
		{"buffer": 0, "byteOffset": indices_offset, "byteLength": indices_len, "target": 34963},
		{"buffer": 0, "byteOffset": ibm_offset, "byteLength": ibm_len},
		{"buffer": 0, "byteOffset": time_offset, "byteLength": time_len},
		{"buffer": 0, "byteOffset": root_rot_offset, "byteLength": root_rot_len},
		{"buffer": 0, "byteOffset": mid_rot_offset, "byteLength": mid_rot_len},
		{"buffer": 0, "byteOffset": tip_rot_offset, "byteLength": tip_rot_len},
	],
	"accessors": [
		{"bufferView": 0, "componentType": 5126, "count": vertex_count, "type": "VEC3", "min": pos_min, "max": pos_max},
		{"bufferView": 1, "componentType": 5126, "count": vertex_count, "type": "VEC3"},
		{"bufferView": 2, "componentType": 5123, "count": vertex_count, "type": "VEC4"},
		{"bufferView": 3, "componentType": 5126, "count": vertex_count, "type": "VEC4"},
		{"bufferView": 4, "componentType": 5125, "count": index_count, "type": "SCALAR"},
		{"bufferView": 5, "componentType": 5126, "count": 3, "type": "MAT4"},
		{"bufferView": 6, "componentType": 5126, "count": len(ANIM_TIMES), "type": "SCALAR", "min": [min(ANIM_TIMES)], "max": [max(ANIM_TIMES)]},
		{"bufferView": 7, "componentType": 5126, "count": len(root_rotations), "type": "VEC4"},
		{"bufferView": 8, "componentType": 5126, "count": len(mid_rotations), "type": "VEC4"},
		{"bufferView": 9, "componentType": 5126, "count": len(tip_rotations), "type": "VEC4"},
	],
}

with open(GLTF_PATH, "w") as f:
	json.dump(gltf, f, indent=2)

print(f"Wrote {GLTF_PATH}")
print(f"Wrote {BIN_PATH} ({total_buffer_len} bytes)")
print(f"Vertices: {vertex_count}, Indices: {index_count}, Joints: 3, Animation: 'Swing' ({ANIM_TIMES[-1]}s loop)")
