# LDraw parts

This folder holds a subset of the LDraw Parts Library: the bricks, minifig accessories and other parts racers-rebuilt maps onto the original game's car pieces, the minifig torso prints it maps onto driver torsos, the extra pieces its car builder offers, and the sub-files and primitives they use.

- **Work:** the LDraw Parts Library, by the LDraw contributors named in each file's `0 Author:` line
- **Source:** the official library at https://library.ldraw.org/library/official/, downloaded on 2026-11-04. Files are unchanged, in the library's `parts/`, `parts/s/` and `p/` layout.
- **Licences:** each file names its own in its `0 !LICENSE` line: CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/), and for some files CC BY 2.0 as well (https://creativecommons.org/licenses/by/2.0/). The licence texts are in [LICENSE-LDRAW.txt](LICENSE-LDRAW.txt). The library's own terms are at https://www.ldraw.org/docs-main/licenses/legal-info.html.
- **Changes:** converted for racers-rebuilt. At run time the port reads these files into triangle meshes in the game's axes and scale, with smoothed normals. Edge lines are dropped. Torso prints are drawn flat into the driver's torso image.

These files are data shipped alongside racers-rebuilt. They aren't part of its GPL-3.0 code.

LDraw is a completely unofficial, community run free CAD system which represents official parts produced by the LEGO company.

LDraw™ is a trademark owned and licensed by the Estate of James Jessiman.

LEGO® is a registered trademark of the LEGO Group, which does not sponsor, endorse, or authorize this project.
