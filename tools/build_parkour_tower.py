"""Generate the industrial parkour blockout; replace only ParkourShowcase entities.

Run with --check to validate the saved course without writing it.
"""
import argparse
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCENE = ROOT / "assets/models/akaza animations/akaza player2.json"
TAG = "ParkourShowcase"
TOWER_SCALE = 2.0
TOWER_ANCHOR = (28.0, 0.0, 16.0)
TOWER_CENTER = (80.0, 0.0, 16.0)
STEEL = [0.18, 0.23, 0.28]
DARK = [0.07, 0.09, 0.12]
CYAN = [0.12, 0.65, 0.75]
AMBER = [0.95, 0.51, 0.08]
RED = [0.78, 0.12, 0.08]


def generate(first_id):
	entities = []

	def box(name, p, s, color=STEEL, solid=True, end=None, speed=1.5, wait=0.8):
		entity = {
			"id": first_id + len(entities), "name": "Tower_" + name, "tag": TAG, "layer": 0,
			"Transform": {"position": p, "rotation": [0, 0, 0], "scale": s, "parentID": 0},
			"MeshComponent": {"assetPath": "primitive_cube", "isSkinned": False},
			"MeshRenderer": {"visible": True, "albedo": color, "shininess": 64,
							 "useTexture": False, "shaderVertexPath": "shaders/arena_lit.vert",
							 "shaderFragmentPath": "shaders/lit.frag"},
		}
		if solid:
			entity["Rigidbody"] = {"isKinematic": True, "useGravity": False, "velocity": [0, 0, 0]}
			entity["BoxCollider"] = {"center": [0, 0, 0], "halfExtents": [0.5, 0.5, 0.5], "isTrigger": False}
		else:
			entity["BoundingSphere"] = {"center": [0, 0, 0], "radius": 0.8660254, "isTrigger": True}
		if end is not None:
			entity["MovingPlatformComponent"] = {
				"type": 0, "active": True, "pingPong": True, "autoReturn": True,
				"startPosition": p, "endPosition": end, "speed": speed, "waitTime": wait,
			}
		entities.append(entity)
		return entity

	# Open structural cage: no full floors or enclosing walls that hide traversal.
	for x in (18, 38):
		for z in (6, 26):
			box(f"Column_{x}_{z}", [x, 13, z], [0.65, 26, 0.65], DARK)
			box(f"Footing_{x}_{z}", [x, 0.3, z], [1.3, 0.6, 1.3], STEEL)
	for y in (6, 12, 18, 25):
		for z in (6, 26):
			box(f"FrameX_{y}_{z}", [28, y, z], [20, 0.35, 0.35], DARK)
		for x in (18, 38):
			box(f"FrameZ_{y}_{x}", [x, y, 16], [0.35, 0.35, 20], DARK)
	box("Entry", [21, -0.2, 9], [4, 0.4, 4], AMBER)
	# Alternate opposite tower faces. Each exit is the next tier's entrance.
	for tier in range(4):
		base = tier * 6
		sign = 1 if tier % 2 == 0 else -1

		def pos(x, y, z):
			return [28 + sign * x, base + y, 16 + sign * z]

		def part(name, x, y, z, sx, sy, sz, color=STEEL, **kwargs):
			return box(f"T{tier + 1}_{name}", pos(x, y, z), [sx, sy, sz], color, **kwargs)

		part("Launch", -7, 1.0, -5, 1.8, 0.4, 2, CYAN)
		part("RunWall", -8.25, 3, -1.5, 0.5, 6, 11, STEEL)
		part("WallStripe", -7.988, 2, -1.5, 0.02, 0.12, 10, CYAN, solid=False)
		part("Catch", -7, 2.2, 0, 1.6, 0.4, 2, CYAN)
		part("KickLanding", -4.8, 3.4, 3, 1.8, 0.4, 1.8, AMBER)
		# Boarding gap is 0.6m. Wait for the shuttle at its near endpoint.
		part("Shuttle", -4.8, 3.4, 5.5, 2, 0.4, 2, CYAN,
			 end=pos(3, 3.4, 5.5), speed=1.8 + tier * 0.15, wait=0.75)
		part("ShuttleRail", -0.9, 2.9, 5.5, 10, 0.15, 0.15, DARK, solid=False)
		part("Transfer", 5, 4.6, 5.5, 2, 0.4, 2, AMBER)
		part("ServiceDeck", 7, 5.8, 7, 3, 0.4, 3, AMBER)
		# A second moving option links the interior shaft to the exterior rest deck.
		part("CoreBridge", 1.7, 5.8, 4, 1.6, 0.4, 2, CYAN,
			 end=pos(6.5, 5.8, 4), speed=1.2, wait=1.1)
		# Narrow maintenance lane: vault, then sprint-slide/dodge through a timed gate.
		part("MaintenanceLane", -3.5, -0.2, -4.5, 2.2, 0.4, 9, STEEL)
		part("EntryLink", -5.2, -0.2, -7, 2.8, 0.4, 2, STEEL)
		part("Vault", -3.5, 0.5, -6, 2.2, 1, 0.35, AMBER)
		part("Gate", -3.5, 4.4, -2.5, 2.2, 2.4, 0.45, RED,
			 end=pos(-3.5, 1.2, -2.5), speed=1.6, wait=0.9)
		part("GateHousing", -3.5, 5.9, -2.5, 2.7, 0.5, 1, DARK)
		part("CoreEntry", -1.7, -0.2, -3.5, 2.8, 0.4, 2, STEEL)
		# Visible hazard pocket and warning bands stay outside the moving sweep.
		for z in (-3.5, -1.5):
			part(f"Warning_{z}", -3.5, 0.012, z, 2.2, 0.024, 0.18, AMBER, solid=False)
		for dx in (-0.85, 0.85):
			part(f"DeckBeacon_{dx}", 7 + dx, 6.03, 8.1, 0.16, 0.06, 0.5, CYAN, solid=False)

	# Interior jump shaft: alternating 1.5m rises with narrow, staggered ledges.
	for x in (25.5, 30.5):
		box(f"CoreWall_{x}", [x, 12, 16], [0.5, 24, 5], STEEL)
		for y in (3, 9, 15, 21):
			box(f"CoreRib_{x}_{y}", [x, y, 18.65], [0.75, 0.2, 0.3], AMBER, solid=False)
	for i in range(16):
		x = 26.4 if i % 2 == 0 else 29.6
		z = 16.8 if (i // 2) % 2 == 0 else 15.2
		box(f"CoreLedge_{i + 1:02}", [x, (i + 1) * 1.5 - 0.2, z], [1.3, 0.4, 1.25], CYAN)
	box("Crown", [28, 23.8, 16], [4.4, 0.4, 4.4], AMBER)
	box("CrownExit", [24, 23.8, 12], [2, 0.4, 2], CYAN)
	box("CrownLink", [22, 23.8, 10], [2, 0.4, 2], CYAN)
	# Machinery silhouette and route-facing louvers, with openings kept clear.
	for x in (32, 33):
		box(f"Duct_{x}", [x, 12, 12], [0.6, 24, 0.6], DARK)
	for y in (2, 8, 14, 20):
		box(f"MachineBank_{y}", [35, y, 13], [3, 2.5, 2], DARK)
		for offset in (-0.6, 0, 0.6):
			box(f"Louver_{y}_{offset}", [35 + offset, y, 11.98], [0.15, 2, 0.04], STEEL, solid=False)
	for z in (6, 26):
		box(f"CrownBeam_{z}", [28, 26, z], [22, 0.6, 0.6], STEEL)
	def scaled_position(position):
		return [center + (value - anchor) * TOWER_SCALE
				for anchor, center, value in zip(TOWER_ANCHOR, TOWER_CENTER, position)]

	for entity in entities:
		transform = entity["Transform"]
		transform["position"] = scaled_position(transform["position"])
		transform["scale"] = [value * TOWER_SCALE for value in transform["scale"]]
		if "MovingPlatformComponent" in entity:
			mover = entity["MovingPlatformComponent"]
			mover["startPosition"] = scaled_position(mover["startPosition"])
			mover["endPosition"] = scaled_position(mover["endPosition"])
			mover["speed"] *= TOWER_SCALE
	box("Foundation", [80, -1.3, 16], [48, 1, 46], DARK)
	return entities


def validate(scene):
	ids = [e["id"] for e in scene["entities"]]
	assert len(ids) == len(set(ids)), "Duplicate entity IDs"
	course = [e for e in scene["entities"] if e.get("tag") == TAG]
	assert course and len({e["name"] for e in course}) == len(course)
	movers = []
	for e in course:
		t = e["Transform"]
		assert all(math.isfinite(v) for v in t["position"] + t["scale"])
		assert all(v > 0 for v in t["scale"])
		if "BoxCollider" in e:
			assert e["BoxCollider"]["halfExtents"] == [0.5] * 3
			assert e["Rigidbody"]["isKinematic"] and not e["Rigidbody"]["useGravity"]
		else:
			assert e["BoundingSphere"]["isTrigger"]
		if "MovingPlatformComponent" in e:
			m = e["MovingPlatformComponent"]
			movers.append(e)
			assert t["position"] == m["startPosition"]
			assert m["startPosition"] != m["endPosition"] and 0 < m["speed"] <= 2.5 * TOWER_SCALE
			assert m["active"] and m["pingPong"] and m["waitTime"] > 0
	assert len(movers) == 12, "Expected four shuttles, four bridges, four timed gates"
	by_name = {e["name"]: e for e in course}
	for tier in range(1, 5):
		launch = by_name[f"Tower_T{tier}_Launch"]["Transform"]
		catch = by_name[f"Tower_T{tier}_Catch"]["Transform"]
		gap = abs(launch["position"][2] - catch["position"][2]) - (launch["scale"][2] + catch["scale"][2]) / 2
		assert 2.8 * TOWER_SCALE <= gap <= 3.2 * TOWER_SCALE
		assert abs(catch["position"][1] - launch["position"][1] - 1.2 * TOWER_SCALE) < 0.001
		gate = by_name[f"Tower_T{tier}_Gate"]["Transform"]
		assert abs(gate["position"][1] - gate["scale"][1] / 2 - ((tier - 1) * 6 + 3.2) * TOWER_SCALE) < 0.001
	assert len([e for e in course if e["name"].startswith("Tower_CoreLedge_")]) == 16
	print(f"Validated {len(course)} tower entities, {len(movers)} movers, summit {24 * TOWER_SCALE:g}m")


def replace_course(text, new_entities):
	# Preserve the exact JSON of every entity outside the replaced course.
	decoder = json.JSONDecoder()
	key = text.index('"entities"')
	start = text.index('[', key)
	cursor = start + 1
	kept = []
	while True:
		while text[cursor].isspace() or text[cursor] == ',':
			cursor += 1
		if text[cursor] == ']':
			break
		entity, end = decoder.raw_decode(text, cursor)
		if entity.get("tag") != TAG:
			kept.append(text[cursor:end])
		cursor = end
	encoded = [json.dumps(e, indent=4) for e in new_entities]
	return text[:start + 1] + '\n        ' + ',\n        '.join(kept + encoded) + '\n    ' + text[cursor:]


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument('--check', action='store_true')
	args = parser.parse_args()
	text = SCENE.read_text(encoding='utf-8-sig')
	scene = json.loads(text)
	if args.check:
		validate(scene)
		return
	kept = [e for e in scene['entities'] if e.get('tag') != TAG]
	first_id = max(400, max(e['id'] for e in kept) + 1)
	updated = replace_course(text, generate(first_id))
	result = json.loads(updated)
	assert [e for e in result['entities'] if e.get('tag') != TAG] == kept
	validate(result)
	SCENE.write_text(updated, encoding='utf-8')


if __name__ == '__main__':
	main()
