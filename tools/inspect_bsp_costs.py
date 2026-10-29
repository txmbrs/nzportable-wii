"""Read-only BSP29/30 geometry/PVS inventory; does not run the engine."""
import collections
import json
import re
import struct
import sys
from pathlib import Path


def inspect(path):
    data = Path(path).read_bytes()
    version, = struct.unpack_from('<i', data)
    if version not in (29, 30):
        raise ValueError(f'Unsupported BSP: {version}')
    lumps = [data[o:o+n] for o, n in struct.iter_unpack('<ii', data[4:124])]
    faces = list(struct.iter_unpack('<HHiHH4Bi', lumps[7]))
    models = list(struct.iter_unpack('<9f7i', lumps[14]))
    # Model 0 owns the world leaf count. Inline models share the leaf lump.
    world_leaves, first_face, world_faces = models[0][-3:]
    leaves = list(struct.iter_unpack('<ii6hHH4B', lumps[10]))
    marks = list(struct.unpack('<' + 'H' * (len(lumps[11]) // 2), lumps[11]))
    entities = re.findall(r'\{([^{}]*)\}', lumps[0].decode('latin1'))
    classes = collections.Counter()
    for entity in entities:
        pairs = dict(re.findall(r'"([^"]*)"\s*"([^"]*)"', entity))
        classes[pairs.get('classname', '?')] += 1
    texture_count, = struct.unpack_from('<i', lumps[2])
    textures = []
    for offset, in struct.iter_unpack('<i', lumps[2][4:4+texture_count*4]):
        if offset < 0:
            continue
        name, w, h = struct.unpack_from('<16sII', lumps[2], offset)
        textures.append((name.split(b'\0')[0].decode('latin1'), w, h))
    visible_leaves, visible_faces = [], []
    for leaf in leaves[1:world_leaves+1]:
        vis_offset = leaf[1]
        if vis_offset < 0:
            bits = bytes([255]) * ((world_leaves+7)//8)
        else:
            bits = bytearray()
            while len(bits) < (world_leaves+7)//8:
                value = lumps[4][vis_offset]
                vis_offset += 1
                if value:
                    bits.append(value)
                else:
                    bits.extend(bytes(lumps[4][vis_offset]))
                    vis_offset += 1
        ids = [i+1 for i in range(world_leaves) if bits[i//8] & (1 << (i%8))]
        surfaces = set()
        for i in ids:
            start, count = leaves[i][8:10]
            surfaces.update(marks[start:start+count])
        visible_leaves.append(len(ids))
        visible_faces.append(len(surfaces))
    def animated(items):
        return sum(any(s not in (0, 255) for s in face[5:9]) for face in items)
    def stats(values):
        return dict(mean=round(sum(values)/len(values), 2), max=max(values)) if values else None
    return dict(path=str(path), version=version, faces=len(faces), world_faces=world_faces,
                brushmodels=len(models)-1, entities=dict(classes.most_common()),
                textures=len(textures), max_texture=[max(t[1] for t in textures), max(t[2] for t in textures)],
                base_rgba_bytes=sum(w*h*4 for _, w, h in textures),
                animated_faces=animated(faces), animated_world_faces=animated(faces[first_face:first_face+world_faces]),
                world_leaves=world_leaves, pvs_leaves=stats(visible_leaves), pvs_faces=stats(visible_faces))


if __name__ == '__main__':
    print(json.dumps([inspect(p) for p in sys.argv[1:]], indent=2))
