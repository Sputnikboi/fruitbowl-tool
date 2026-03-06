"""
Z-fighting scanner for Minecraft resource pack models.
Detects overlapping coplanar faces between elements in a model.
"""

import json
import os
from dataclasses import dataclass


@dataclass
class Face:
    """A face of an element, defined by its plane and 2D bounds."""
    element_idx: int
    element_name: str
    face_name: str       # north, south, east, west, up, down
    axis: str            # x, y, or z — the axis normal to this face
    plane_coord: float   # position on that axis
    min_a: float         # min on first tangent axis
    max_a: float         # max on first tangent axis
    min_b: float         # min on second tangent axis
    max_b: float         # max on second tangent axis


def _get_faces(element: dict, idx: int) -> list[Face]:
    """Extract all 6 possible faces from an element as Face objects."""
    fr = element["from"]
    to = element["to"]
    name = element.get("name", f"element_{idx}")
    faces = []

    # Map face names to their plane axis, coordinate, and 2D tangent bounds
    # (axis, plane_coord, min_a, max_a, min_b, max_b)
    face_defs = {
        "north": ("z", fr[2], fr[0], to[0], fr[1], to[1]),  # -Z face
        "south": ("z", to[2], fr[0], to[0], fr[1], to[1]),  # +Z face
        "west":  ("x", fr[0], fr[2], to[2], fr[1], to[1]),  # -X face
        "east":  ("x", to[0], fr[2], to[2], fr[1], to[1]),  # +X face
        "down":  ("y", fr[1], fr[0], to[0], fr[2], to[2]),  # -Y face
        "up":    ("y", to[1], fr[0], to[0], fr[2], to[2]),  # +Y face
    }

    for face_name, (axis, coord, min_a, max_a, min_b, max_b) in face_defs.items():
        if face_name in element.get("faces", {}):
            faces.append(Face(
                element_idx=idx,
                element_name=name,
                face_name=face_name,
                axis=axis,
                plane_coord=coord,
                min_a=min(min_a, max_a),
                max_a=max(min_a, max_a),
                min_b=min(min_b, max_b),
                max_b=max(min_b, max_b),
            ))

    return faces


def _rects_overlap(f1: Face, f2: Face, tolerance: float = 0.001) -> bool:
    """Check if two faces on the same plane have overlapping 2D rectangles."""
    # No overlap if separated on either tangent axis
    if f1.max_a <= f2.min_a + tolerance or f2.max_a <= f1.min_a + tolerance:
        return False
    if f1.max_b <= f2.min_b + tolerance or f2.max_b <= f1.min_b + tolerance:
        return False
    return True


def _overlap_area(f1: Face, f2: Face) -> float:
    """Compute the area of overlap between two coplanar faces."""
    oa = max(0, min(f1.max_a, f2.max_a) - max(f1.min_a, f2.min_a))
    ob = max(0, min(f1.max_b, f2.max_b) - max(f1.min_b, f2.min_b))
    return oa * ob


@dataclass
class ZFightHit:
    """A pair of faces that are z-fighting."""
    elem1_idx: int
    elem1_name: str
    face1: str
    elem2_idx: int
    elem2_name: str
    face2: str
    axis: str
    plane_coord: float
    overlap_area: float


def scan_model(model: dict) -> list[ZFightHit]:
    """
    Scan a single model JSON for z-fighting faces.
    Returns a list of ZFightHit for each pair of overlapping coplanar faces.
    """
    elements = model.get("elements", [])
    if len(elements) < 2:
        return []

    # Collect all faces
    all_faces = []
    for i, el in enumerate(elements):
        all_faces.extend(_get_faces(el, i))

    # Group faces by (axis, plane_coord) — only same-plane faces can z-fight
    from collections import defaultdict
    planes = defaultdict(list)
    for face in all_faces:
        # Round to avoid float noise
        key = (face.axis, round(face.plane_coord, 4))
        planes[key].append(face)

    # Check for overlaps within each plane group
    hits = []
    for (axis, coord), faces in planes.items():
        for i in range(len(faces)):
            for j in range(i + 1, len(faces)):
                f1, f2 = faces[i], faces[j]
                # Skip faces from the same element
                if f1.element_idx == f2.element_idx:
                    continue
                if _rects_overlap(f1, f2):
                    area = _overlap_area(f1, f2)
                    if area > 0.001:  # ignore tiny overlaps
                        hits.append(ZFightHit(
                            elem1_idx=f1.element_idx,
                            elem1_name=f1.element_name,
                            face1=f1.face_name,
                            elem2_idx=f2.element_idx,
                            elem2_name=f2.element_name,
                            face2=f2.face_name,
                            axis=axis,
                            plane_coord=coord,
                            overlap_area=area,
                        ))

    return hits


def scan_pack(pack_root: str) -> dict[str, list[ZFightHit]]:
    """
    Scan all models in a resource pack for z-fighting.
    Returns a dict mapping model path (relative) to its list of ZFightHits.
    Only models with hits are included.
    """
    results = {}
    models_dir = os.path.join(pack_root, "assets", "fruitbowl", "models", "item")

    if not os.path.isdir(models_dir):
        return results

    for filename in sorted(os.listdir(models_dir)):
        if not filename.endswith(".json"):
            continue

        filepath = os.path.join(models_dir, filename)
        try:
            with open(filepath, "r", encoding="utf-8") as f:
                model = json.load(f)
        except (json.JSONDecodeError, OSError):
            continue

        hits = scan_model(model)
        if hits:
            results[filename] = hits

    return results


def format_report(results: dict[str, list[ZFightHit]]) -> str:
    """Format scan results into a human-readable report."""
    if not results:
        return "✓ No z-fighting detected in any models."

    lines = [f"⚠ Z-fighting detected in {len(results)} model(s):", ""]

    for model_name, hits in results.items():
        lines.append(f"  {model_name} — {len(hits)} overlap(s):")
        for hit in hits:
            lines.append(
                f"    [{hit.elem1_idx}] {hit.elem1_name}/{hit.face1}"
                f"  ↔  [{hit.elem2_idx}] {hit.elem2_name}/{hit.face2}"
                f"  (plane {hit.axis}={hit.plane_coord:.2f},"
                f" overlap {hit.overlap_area:.3f})")
        lines.append("")

    return "\n".join(lines)
