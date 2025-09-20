import os
import subprocess
from lxml import etree
import re


################ Convert SVG to PNG ############################################
def extract_translation(transform):
    m = re.search(r'translate\(\s*(-?\d+(?:\.\d+)?)[ ,]+(-?\d+(?:\.\d+)?)\s*\)', transform)
    if m:
        return float(m.group(1)), float(m.group(2))
    return 0.0, 0.0



def extract_group_segs(svg_path_list, output_dir, INKSCAPE_PATH):
    os.makedirs(output_dir, exist_ok=True)

    for i_path in range(len(svg_path_list)):
        svg_path = svg_path_list[i_path]
        tree = etree.parse(svg_path)
        root = tree.getroot()
        ns = {'svg': 'http://www.w3.org/2000/svg',
              'inkscape': 'http://www.inkscape.org/namespaces/inkscape'}

        segs_group = root.find('.//svg:g[@inkscape:label="segs"]', ns)
        if segs_group is None:
            print("ERROR NOT FIND SEG GROUP IN SVG")
            return
        tx, ty = extract_translation(segs_group.attrib.get('transform', ''))    
        
        compteur = 1
        for element in segs_group:
            title_elem = element.find('svg:title', ns)
            if title_elem is not None and title_elem.text:
                nom_base = title_elem.text.strip() + '.' + str(i_path) 
                if not os.path.exists(os.path.join(output_dir, f'{nom_base}.png')):
                    new_svg = etree.Element(root.tag, nsmap=root.nsmap)
                    
                    for attr in ['width', 'height', 'viewBox']:
                        if attr == 'viewBox' and attr in root.attrib:
                            vb = root.attrib[attr].split()
                            min_x, min_y, w, h = map(float, vb)
                            min_x -= tx
                            min_y -= ty
                            new_svg.attrib[attr] = f"{min_x} {min_y} {w} {h}"
                        elif attr in root.attrib:
                            new_svg.attrib[attr] = root.attrib[attr]
                                
                    clone = etree.Element(element.tag, nsmap=element.nsmap)
                    clone.attrib.update(element.attrib)
                    for child in element: clone.append(child)
                    new_svg.append(clone)

                    svg_temp_path = os.path.join(output_dir, f'{nom_base}.svg')
                    png_path = os.path.join(output_dir, f'{nom_base}.png')

                    with open(svg_temp_path, 'wb') as f: f.write(etree.tostring(new_svg, pretty_print=True))

                    # Fallback export simple
                    subprocess.run([ INKSCAPE_PATH, svg_temp_path,
                        '--export-type=png', f'--export-filename={png_path}', f'--export-dpi=50' ])

                    print(f' -- Export : {png_path}')
                    compteur += 1
                    os.remove(svg_temp_path)


