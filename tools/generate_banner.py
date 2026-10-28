#!/usr/bin/env python3
"""
tools/generate_banner.py

Generador de recursos gráficos y modelos 3D para osu! 3DS (HOME Menu):
1. banner.png (256x128 RGBA, fondo transparente para efecto estereoscópico 3D)
2. icon.png (48x48 RGBA, icono 3D en relieve de alta nitidez)
3. osu_medallion_3d.obj y osu_medallion_3d.mtl (Malla 3D low-poly para 3DS/CGFX)

Autor: Acira345 / osu! 3DS Project
"""

import os
import sys
import math
import argparse
from PIL import Image, ImageDraw, ImageFont, ImageFilter


def find_best_font(font_names, size, bold=True):
    """Busca una fuente TTF disponible en el sistema operativo con fallback seguro."""
    candidate_paths = []
    
    # Fuentes directas o en Windows Fonts
    win_fonts = os.environ.get("WINDIR", "C:\\Windows") + "\\Fonts"
    for name in font_names:
        candidate_paths.append(name)
        candidate_paths.append(os.path.join(win_fonts, name))
    
    for path in candidate_paths:
        try:
            return ImageFont.truetype(path, size)
        except Exception:
            continue
    
    # Fallback si no encuentra ninguna de las preferidas
    try:
        return ImageFont.truetype("arial.ttf", size)
    except Exception:
        return ImageFont.load_default()


def render_medallion_3d(radius_px, thickness_px, pitch_deg=17.0, yaw_deg=-11.0,
                         scale=2, text="osu!", text_size_ratio=0.88):
    """
    Renderiza un medallón 3D volumétrico con ray-casting y sombreado Phong.
    - Perspectiva ligeramente inclinada (pitch y yaw)
    - Canto lateral con grosor tridimensional
    - Bisel blanco brillante con sombreado suave
    - Centro rosa #FF66AA con gradiente de iluminación especular
    - Texto 'osu!' con sombra proyectada integrado en la superficie
    """
    R = float(radius_px * scale)
    T = float(thickness_px * scale)
    R_in = 0.725 * R
    
    # Dimensiones del lienzo local
    margin = int(24 * scale)
    W = int(R * 2.5 + margin * 2)
    H = int(R * 2.5 + margin * 2)
    cx = W / 2.0
    cy = H / 2.0 - T * 0.25  # compensar grosor inferior
    
    # Ángulos de rotación
    pitch = math.radians(pitch_deg)
    yaw = math.radians(yaw_deg)
    
    cp, sp = math.cos(pitch), math.sin(pitch)
    cyaw, syaw = math.cos(yaw), math.sin(yaw)
    
    # Matriz de rotación local -> vista
    r00, r01, r02 = cyaw, 0.0, syaw
    r10, r11, r12 = sp * syaw, cp, -sp * cyaw
    r20, r21, r22 = -cp * syaw, sp, cp * cyaw
    
    # Vector de dirección del rayo en espacio local (vista ortográfica paralela)
    Dx = -r20
    Dy = -r21
    Dz = -r22
    
    # Luz principal (arriba-izquierda, ligeramente frontal)
    Lx, Ly, Lz = -0.55, -0.65, 0.52
    l_len = math.sqrt(Lx * Lx + Ly * Ly + Lz * Lz)
    Lx, Ly, Lz = Lx / l_len, Ly / l_len, Lz / l_len
    
    # Vector intermedio Blinn-Phong (Vista = (0, 0, 1))
    Hx, Hy, Hz = Lx, Ly, Lz + 1.0
    h_len = math.sqrt(Hx * Hx + Hy * Hy + Hz * Hz)
    Hx, Hy, Hz = Hx / h_len, Hy / h_len, Hz / h_len
    
    # 1. Crear la textura 2D del texto con sombra proyectada en el plano local del disco
    tex_res = int(R_in * 2.4)
    if tex_res < 64:
        tex_res = 64
    tex = Image.new("RGBA", (tex_res, tex_res), (0, 0, 0, 0))
    tdraw = ImageDraw.Draw(tex)
    
    font_size = int(tex_res * 0.42 * text_size_ratio)
    font = find_best_font(["segoeuib.ttf", "arialbd.ttf", "trebucbd.ttf", "DejaVuSans-Bold.ttf"], font_size)
    
    # Medir texto
    bbox = tdraw.textbbox((0, 0), text, font=font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    tx = (tex_res - tw) / 2.0 - bbox[0]
    ty = (tex_res - th) / 2.0 - bbox[1] - tex_res * 0.02
    
    # Capa de sombra suave del texto sobre el medallón rosa
    shadow_offset_x = max(1.0, 2.5 * scale)
    shadow_offset_y = max(1.5, 4.0 * scale)
    slayer = Image.new("RGBA", (tex_res, tex_res), (0, 0, 0, 0))
    sdraw = ImageDraw.Draw(slayer)
    sdraw.text((tx + shadow_offset_x, ty + shadow_offset_y), text, fill=(0, 0, 0, 175), font=font)
    slayer = slayer.filter(ImageFilter.GaussianBlur(max(1.0, 2.0 * scale)))
    tex.alpha_composite(slayer)
    
    # Capa de texto principal (blanco con ligera calidez)
    tlayer = Image.new("RGBA", (tex_res, tex_res), (0, 0, 0, 0))
    tldraw = ImageDraw.Draw(tlayer)
    tldraw.text((tx, ty), text, fill=(255, 255, 255, 255), font=font)
    tex.alpha_composite(tlayer)
    
    tex_bytes = tex.tobytes()
    
    # 2. Ray-casting per-pixel
    buf = bytearray(W * H * 4)
    idx = 0
    
    # Caja delimitadora en espacio de pantalla
    bb_margin = R * 1.35
    min_px = max(0, int(cx - bb_margin))
    max_px = min(W, int(cx + bb_margin))
    min_py = max(0, int(cy - bb_margin))
    max_py = min(H, int(cy + bb_margin + T * 1.5))
    
    for py in range(H):
        if py < min_py or py >= max_py:
            idx += W * 4
            continue
        
        vy = py - cy
        p_row_start = idx + min_px * 4
        
        for px in range(min_px, max_px):
            vx = px - cx
            p_idx = idx + px * 4
            
            # Origen del rayo en espacio local
            Ox = vx * r00 + vy * r10
            Oy = vx * r01 + vy * r11
            Oz = vx * r02 + vy * r12
            
            hit = False
            t_hit = 1e9
            mat_id = 0  # 1: disco rosa, 2: bisel blanco, 3: canto lateral
            fx, fy = 0.0, 0.0
            nx, ny, nz = 0.0, 0.0, 1.0
            
            # Intersección 1: Cara frontal (Z_loc = 0)
            if abs(Dz) > 1e-6:
                tf = -Oz / Dz
                fx_test = Ox + tf * Dx
                fy_test = Oy + tf * Dy
                fr2 = fx_test * fx_test + fy_test * fy_test
                if fr2 <= R * R:
                    hit = True
                    t_hit = tf
                    fx, fy = fx_test, fy_test
                    fr = math.sqrt(fr2)
                    
                    if fr < R_in:
                        mat_id = 1  # Centro rosa
                        # Cúpula esférica muy suave para el reflejo especular
                        dh_dr = -0.045 * (fr / R_in)
                        lnx = -dh_dr * (fx / (fr + 1e-6))
                        lny = -dh_dr * (fy / (fr + 1e-6))
                        lnz = 1.0
                    else:
                        mat_id = 2  # Bisel blanco brillante
                        w_bevel = (R - R_in) * 0.5
                        rc = (R + R_in) * 0.5
                        delta = max(-0.95, min(0.95, (fr - rc) / w_bevel))
                        dh_dr = -0.38 * delta / math.sqrt(1.0 - delta * delta + 1e-4)
                        lnx = -dh_dr * (fx / (fr + 1e-6))
                        lny = -dh_dr * (fy / (fr + 1e-6))
                        lnz = 1.0
                    
                    # Normal en espacio de cámara
                    l_len = math.sqrt(lnx * lnx + lny * lny + lnz * lnz)
                    lnx, lny, lnz = lnx / l_len, lny / l_len, lnz / l_len
                    nx = r00 * lnx + r01 * lny + r02 * lnz
                    ny = r10 * lnx + r11 * lny + r12 * lnz
                    nz = r20 * lnx + r21 * lny + r22 * lnz
            
            # Intersección 2: Canto cilíndrico lateral (-T <= Z_loc <= 0)
            A_cyl = Dx * Dx + Dy * Dy
            B_cyl = 2.0 * (Ox * Dx + Oy * Dy)
            C_cyl = Ox * Ox + Oy * Oy - R * R
            disc = B_cyl * B_cyl - 4.0 * A_cyl * C_cyl
            
            if disc >= 0 and A_cyl > 1e-6:
                sqd = math.sqrt(disc)
                for tc in ((-B_cyl - sqd) / (2.0 * A_cyl), (-B_cyl + sqd) / (2.0 * A_cyl)):
                    if tc < t_hit:
                        zc = Oz + tc * Dz
                        if -T <= zc <= 0:
                            hit = True
                            t_hit = tc
                            mat_id = 3  # Canto lateral
                            cx_loc = Ox + tc * Dx
                            cy_loc = Oy + tc * Dy
                            lnx = cx_loc / R
                            lny = cy_loc / R
                            lnz = 0.0
                            nx = r00 * lnx + r01 * lny + r02 * lnz
                            ny = r10 * lnx + r11 * lny + r12 * lnz
                            nz = r20 * lnx + r21 * lny + r22 * lnz
                            break
            
            if hit:
                # Iluminación Blinn-Phong
                diffuse = max(0.0, nx * Lx + ny * Ly + nz * Lz)
                ndoth = max(0.0, nx * Hx + ny * Hy + nz * Hz)
                
                if mat_id == 1:
                    # Centro rosa (#FF66AA) con iluminación y textura proyectada
                    spec = math.pow(ndoth, 24.0) * 0.72
                    amb = 0.38
                    shade = amb + diffuse * 0.62
                    
                    # Gradiente rosa vibrante
                    pr = min(255, int(255 * shade + 255 * spec))
                    pg = min(255, int(102 * shade + 255 * spec))
                    pb = min(255, int(170 * shade + 255 * spec))
                    
                    # Muestreo de textura local
                    u = (fx / R_in + 1.0) * 0.5
                    v = (fy / R_in + 1.0) * 0.5
                    if 0.0 <= u <= 1.0 and 0.0 <= v <= 1.0:
                        tu = max(0, min(tex_res - 1, int(u * tex_res)))
                        tv = max(0, min(tex_res - 1, int(v * tex_res)))
                        t_offset = (tv * tex_res + tu) * 4
                        tr = tex_bytes[t_offset]
                        tg = tex_bytes[t_offset + 1]
                        tb = tex_bytes[t_offset + 2]
                        ta = tex_bytes[t_offset + 3]
                        
                        if ta > 0:
                            alpha_f = ta / 255.0
                            if tr == 0 and tg == 0 and tb == 0:
                                # Sombra arrojada del texto sobre el rosa
                                s_factor = 1.0 - alpha_f * 0.52
                                pr = int(pr * s_factor)
                                pg = int(pg * s_factor)
                                pb = int(pb * s_factor)
                            else:
                                # Texto blanco con relieve suave
                                tw_r = min(255, int(tr * (0.86 + diffuse * 0.14) + 255 * spec * 0.35))
                                tw_g = min(255, int(tg * (0.86 + diffuse * 0.14) + 255 * spec * 0.35))
                                tw_b = min(255, int(tb * (0.86 + diffuse * 0.14) + 255 * spec * 0.35))
                                pr = int(pr * (1.0 - alpha_f) + tw_r * alpha_f)
                                pg = int(pg * (1.0 - alpha_f) + tw_g * alpha_f)
                                pb = int(pb * (1.0 - alpha_f) + tw_b * alpha_f)
                    
                    buf[p_idx] = pr
                    buf[p_idx + 1] = pg
                    buf[p_idx + 2] = pb
                    buf[p_idx + 3] = 255
                
                elif mat_id == 2:
                    # Bisel blanco brillante (#FFFFFF / #F0F0F8)
                    spec = math.pow(ndoth, 34.0) * 0.95
                    amb = 0.62
                    shade = amb + diffuse * 0.38
                    buf[p_idx] = min(255, int(250 * shade + 255 * spec))
                    buf[p_idx + 1] = min(255, int(250 * shade + 255 * spec))
                    buf[p_idx + 2] = min(255, int(253 * shade + 255 * spec))
                    buf[p_idx + 3] = 255
                
                elif mat_id == 3:
                    # Canto lateral 3D con grosor y contraste
                    spec = math.pow(ndoth, 16.0) * 0.45
                    amb = 0.22
                    shade = amb + diffuse * 0.78
                    buf[p_idx] = min(255, int(195 * shade + 255 * spec))
                    buf[p_idx + 1] = min(255, int(65 * shade + 255 * spec))
                    buf[p_idx + 2] = min(255, int(120 * shade + 255 * spec))
                    buf[p_idx + 3] = 255
        
        idx += W * 4
    
    medallion_img = Image.frombytes("RGBA", (W, H), bytes(buf))
    
    # 3. Sombra de contacto / ambiental inferior flotante (para efecto 3D estereoscópico)
    shadow_img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    s_draw = ImageDraw.Draw(shadow_img)
    sh_rad_x = R * 0.96
    sh_rad_y = R * 0.88 * cp
    sh_cx = cx + 4.0 * scale
    sh_cy = cy + T * 0.75 + 6.0 * scale
    s_draw.ellipse([sh_cx - sh_rad_x, sh_cy - sh_rad_y, sh_cx + sh_rad_x, sh_cy + sh_rad_y],
                   fill=(0, 0, 0, 110))
    shadow_img = shadow_img.filter(ImageFilter.GaussianBlur(max(1.5, 4.0 * scale)))
    
    # Combinar sombra y medallón
    result = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    result.alpha_composite(shadow_img)
    result.alpha_composite(medallion_img)
    
    return result


def create_banner_image(output_path, scale=2):
    """
    Genera banner.png (256x128 px, RGBA) con fondo 100% transparente.
    - Medallón 3D a la izquierda
    - Título 'osu! 3DS' a la derecha en tipografía limpia con relieve/sombra 3D
    - Subtítulo 'Standard Edition' en azul claro (#66CCFF)
    """
    target_w, target_h = 256, 128
    W, H = target_w * scale, target_h * scale
    
    canvas = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    
    # 1. Medallón 3D a la izquierda
    medallion_r = 45.0  # en espacio 256x128
    medallion_thickness = 10.5
    medallion = render_medallion_3d(
        radius_px=medallion_r,
        thickness_px=medallion_thickness,
        pitch_deg=17.0,
        yaw_deg=-10.0,
        scale=scale,
        text="osu!"
    )
    
    # Centrar medallón en X=62, Y=62 (espacio 256x128)
    med_target_cx = int(62 * scale)
    med_target_cy = int(62 * scale)
    paste_x = med_target_cx - medallion.width // 2
    paste_y = med_target_cy - medallion.height // 2
    canvas.alpha_composite(medallion, (paste_x, paste_y))
    
    # 2. Textos a la derecha
    # Área disponible: X desde ~120 hasta 250
    text_canvas = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    tdraw = ImageDraw.Draw(text_canvas)
    
    # Fuentes para alta resolución (escala incluida)
    font_title = find_best_font(["segoeuib.ttf", "arialbd.ttf", "tahomabd.ttf"], int(24 * scale))
    font_sub = find_best_font(["segoeui.ttf", "arial.ttf", "tahoma.ttf"], int(10.5 * scale))
    
    # Coordenadas calculadas
    center_right_x = int(185 * scale)
    
    # Medir Título: 'osu! 3DS'
    title_text = "osu! 3DS"
    t_bbox = tdraw.textbbox((0, 0), title_text, font=font_title)
    t_w = t_bbox[2] - t_bbox[0]
    t_h = t_bbox[3] - t_bbox[1]
    title_x = center_right_x - t_w // 2 - t_bbox[0]
    title_y = int(40 * scale) - t_h // 2 - t_bbox[1]
    
    # Medir Subtítulo: 'Standard Edition'
    sub_text = "Standard Edition"
    s_bbox = tdraw.textbbox((0, 0), sub_text, font=font_sub)
    s_w = s_bbox[2] - s_bbox[0]
    s_h = s_bbox[3] - s_bbox[1]
    sub_x = center_right_x - s_w // 2 - s_bbox[0]
    sub_y = int(82 * scale) - s_h // 2 - s_bbox[1]
    
    # Capa de sombra 3D extruida para el título
    title_shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ts_draw = ImageDraw.Draw(title_shadow)
    # Extrusión en varias pasadas sutiles
    for offset_d in range(1, int(3.5 * scale)):
        ts_draw.text((title_x + offset_d, title_y + offset_d + 1), title_text,
                     fill=(10, 15, 25, 140), font=font_title)
    title_shadow = title_shadow.filter(ImageFilter.GaussianBlur(max(1.0, 1.5 * scale)))
    canvas.alpha_composite(title_shadow)
    
    # Título frontal: Blanco puro brillante con sutil relieve
    t_front = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    tf_draw = ImageDraw.Draw(t_front)
    tf_draw.text((title_x, title_y), title_text, fill=(255, 255, 255, 255), font=font_title)
    canvas.alpha_composite(t_front)
    
    # Línea decorativa divisoria con gradiente suave
    divider_w = int(90 * scale)
    divider_h = max(2, int(2 * scale))
    divider_x = center_right_x - divider_w // 2
    divider_y = int(62 * scale)
    
    div_img = Image.new("RGBA", (divider_w, divider_h), (0, 0, 0, 0))
    for dx in range(divider_w):
        # Gradiente simétrico (máximo al centro, desvanece en bordes)
        norm_dist = abs(dx - divider_w / 2.0) / (divider_w / 2.0)
        alpha = int(max(0, 1.0 - norm_dist) * 200)
        for dy in range(divider_h):
            div_img.putpixel((dx, dy), (102, 204, 255, alpha))
    canvas.alpha_composite(div_img, (divider_x, divider_y))
    
    # Subtítulo: 'Standard Edition' en azul claro (#66CCFF) con sombra de lectura
    sub_shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ss_draw = ImageDraw.Draw(sub_shadow)
    ss_draw.text((sub_x + 1.5 * scale, sub_y + 2.0 * scale), sub_text,
                 fill=(5, 15, 35, 190), font=font_sub)
    sub_shadow = sub_shadow.filter(ImageFilter.GaussianBlur(max(0.8, 1.2 * scale)))
    canvas.alpha_composite(sub_shadow)
    
    sub_front = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sf_draw = ImageDraw.Draw(sub_front)
    # Azul claro vibrante: #66CCFF (RGB 102, 204, 255)
    sf_draw.text((sub_x, sub_y), sub_text, fill=(102, 204, 255, 255), font=font_sub)
    canvas.alpha_composite(sub_front)
    
    # Reducir con filtro Lanczos a 256x128 exactamente para anti-aliasing impecable
    final_banner = canvas.resize((target_w, target_h), Image.Resampling.LANCZOS)
    final_banner.save(output_path, format="PNG")
    print(f"[OK] Generado banner: {output_path} ({target_w}x{target_h} RGBA)")


def create_icon_image(output_path, scale=4):
    """
    Genera icon.png (48x48 px, RGBA) con relieve 3D nítido para el menú HOME.
    """
    target_w, target_h = 48, 48
    icon_medallion = render_medallion_3d(
        radius_px=20.5,
        thickness_px=4.2,
        pitch_deg=14.0,
        yaw_deg=-8.0,
        scale=scale,
        text="osu!",
        text_size_ratio=0.92
    )
    
    # Ajustar en lienzo de 48x48
    canvas_w = int(target_w * scale)
    canvas_h = int(target_h * scale)
    canvas = Image.new("RGBA", (canvas_w, canvas_h), (0, 0, 0, 0))
    
    paste_x = (canvas_w - icon_medallion.width) // 2
    paste_y = (canvas_h - icon_medallion.height) // 2
    canvas.alpha_composite(icon_medallion, (paste_x, paste_y))
    
    final_icon = canvas.resize((target_w, target_h), Image.Resampling.LANCZOS)
    final_icon.save(output_path, format="PNG")
    print(f"[OK] Generado icono:  {output_path} ({target_w}x{target_h} RGBA)")


def create_3d_mesh(obj_path, mtl_path):
    """
    Genera la malla poligonal 3D del medallón biselado (optimizado para PICA200 / 3DS)
    con materiales OsuPink y OsuWhite.
    """
    mtl_filename = os.path.basename(mtl_path)
    
    # 1. Archivo de Materiales (.mtl)
    with open(mtl_path, "w", encoding="utf-8") as f:
        f.write("# osu! 3DS Medallion Material Definitions\n")
        f.write("# Compatible with Nintendo 3DS / CGFX\n\n")
        
        # OsuPink: #FF66AA
        f.write("newmtl OsuPink\n")
        f.write("Ka 0.25 0.10 0.16\n")
        f.write("Kd 1.00 0.40 0.667\n")
        f.write("Ks 0.60 0.60 0.60\n")
        f.write("Ns 40.0\n")
        f.write("illum 2\n\n")
        
        # OsuWhite: Bisel y cantos
        f.write("newmtl OsuWhite\n")
        f.write("Ka 0.20 0.20 0.20\n")
        f.write("Kd 0.95 0.95 0.95\n")
        f.write("Ks 0.85 0.85 0.85\n")
        f.write("Ns 60.0\n")
        f.write("illum 2\n")
    
    print(f"[OK] Generado MTL:    {mtl_path}")
    
    # 2. Archivo Wavefront (.obj)
    N = 36  # Segmentos radiales equilibrados para GPU 3DS
    
    R_outer = 1.0
    R_crest = 0.88
    R_inner = 0.725
    
    Z_back = -0.20
    Z_rim = 0.02
    Z_crest = 0.12
    Z_center = 0.05
    
    verts = []
    norms = []
    uvs = []
    
    def add_vert(x, y, z, u, v, nx, ny, nz):
        verts.append((x, y, z))
        uvs.append((u, v))
        norms.append((nx, ny, nz))
        return len(verts)  # 1-based index
    
    # Vértice central frontal (Disco Rosa)
    v_center_front = add_vert(0.0, 0.0, Z_center, 0.5, 0.5, 0.0, 0.0, 1.0)
    
    # Anillos frontales
    ring_inner = []
    ring_crest = []
    ring_outer = []
    ring_back = []
    
    for i in range(N):
        ang = 2.0 * math.pi * i / N
        ca, sa = math.cos(ang), math.sin(ang)
        
        # 1. Borde interno disco rosa (R_inner)
        u1 = 0.5 + 0.5 * (R_inner / R_outer) * ca
        v1 = 0.5 + 0.5 * (R_inner / R_outer) * sa
        idx_inner = add_vert(R_inner * ca, R_inner * sa, Z_center, u1, v1, 0.0, 0.0, 1.0)
        ring_inner.append(idx_inner)
        
        # 2. Cresta del bisel blanco (R_crest)
        u2 = 0.5 + 0.5 * (R_crest / R_outer) * ca
        v2 = 0.5 + 0.5 * (R_crest / R_outer) * sa
        idx_crest = add_vert(R_crest * ca, R_crest * sa, Z_crest, u2, v2, 0.0, 0.0, 1.0)
        ring_crest.append(idx_crest)
        
        # 3. Borde exterior del bisel (R_outer, Z_rim)
        u3 = 0.5 + 0.5 * ca
        v3 = 0.5 + 0.5 * sa
        idx_outer = add_vert(R_outer * ca, R_outer * sa, Z_rim, u3, v3, 0.5 * ca, 0.5 * sa, 0.707)
        ring_outer.append(idx_outer)
        
        # 4. Borde posterior (R_outer, Z_back)
        idx_back = add_vert(R_outer * ca, R_outer * sa, Z_back, i / float(N), 1.0, ca, sa, 0.0)
        ring_back.append(idx_back)
    
    # Vértice central posterior
    v_center_back = add_vert(0.0, 0.0, Z_back, 0.5, 0.5, 0.0, 0.0, -1.0)
    
    with open(obj_path, "w", encoding="utf-8") as f:
        f.write("# osu! 3DS Medallion 3D Low-Poly Mesh\n")
        f.write(f"mtllib {mtl_filename}\n\n")
        
        # Vértices
        for x, y, z in verts:
            f.write(f"v {x:.5f} {y:.5f} {z:.5f}\n")
        f.write("\n")
        
        # UVs
        for u, v in uvs:
            f.write(f"vt {u:.5f} {v:.5f}\n")
        f.write("\n")
        
        # Normales
        for nx, ny, nz in norms:
            f.write(f"vn {nx:.5f} {ny:.5f} {nz:.5f}\n")
        f.write("\n")
        
        # Caras del disco rosa (OsuPink)
        f.write("g CenterDisc\n")
        f.write("usemtl OsuPink\n")
        for i in range(N):
            next_i = (i + 1) % N
            v0 = v_center_front
            v1 = ring_inner[i]
            v2 = ring_inner[next_i]
            f.write(f"f {v0}/{v0}/{v0} {v1}/{v1}/{v1} {v2}/{v2}/{v2}\n")
        f.write("\n")
        
        # Caras del bisel y cantos (OsuWhite)
        f.write("g BevelAndRim\n")
        f.write("usemtl OsuWhite\n")
        
        # Bisel interno: entre ring_inner y ring_crest
        for i in range(N):
            next_i = (i + 1) % N
            i1 = ring_inner[i]
            i2 = ring_inner[next_i]
            c1 = ring_crest[i]
            c2 = ring_crest[next_i]
            f.write(f"f {i1}/{i1}/{i1} {c1}/{c1}/{c1} {c2}/{c2}/{c2}\n")
            f.write(f"f {i1}/{i1}/{i1} {c2}/{c2}/{c2} {i2}/{i2}/{i2}\n")
        
        # Bisel externo: entre ring_crest y ring_outer
        for i in range(N):
            next_i = (i + 1) % N
            c1 = ring_crest[i]
            c2 = ring_crest[next_i]
            o1 = ring_outer[i]
            o2 = ring_outer[next_i]
            f.write(f"f {c1}/{c1}/{c1} {o1}/{o1}/{o1} {o2}/{o2}/{o2}\n")
            f.write(f"f {c1}/{c1}/{c1} {o2}/{o2}/{o2} {c2}/{c2}/{c2}\n")
        
        # Canto lateral cilíndrico: entre ring_outer y ring_back
        for i in range(N):
            next_i = (i + 1) % N
            o1 = ring_outer[i]
            o2 = ring_outer[next_i]
            b1 = ring_back[i]
            b2 = ring_back[next_i]
            f.write(f"f {o1}/{o1}/{o1} {b1}/{b1}/{b1} {b2}/{b2}/{b2}\n")
            f.write(f"f {o1}/{o1}/{o1} {b2}/{b2}/{b2} {o2}/{o2}/{o2}\n")
        
        # Tapa posterior
        for i in range(N):
            next_i = (i + 1) % N
            b1 = ring_back[i]
            b2 = ring_back[next_i]
            f.write(f"f {v_center_back}/{v_center_back}/{v_center_back} {b2}/{b2}/{b2} {b1}/{b1}/{b1}\n")
    
    print(f"[OK] Generado OBJ:    {obj_path} ({len(verts)} vértices, {N*7} caras)")


def main():
    parser = argparse.ArgumentParser(description="Generador 3D/2D de Banner, Icono y Malla para osu! 3DS")
    parser.add_argument("--banner", default="banner.png", help="Ruta de salida para banner.png")
    parser.add_argument("--icon", default="icon.png", help="Ruta de salida para icon.png")
    parser.add_argument("--obj", default="osu_medallion_3d.obj", help="Ruta de salida para osu_medallion_3d.obj")
    parser.add_argument("--mtl", default="osu_medallion_3d.mtl", help="Ruta de salida para osu_medallion_3d.mtl")
    parser.add_argument("--out-dir", default=os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets"), help="Directorio base de salida (por defecto: assets/ del repo)")
    
    args = parser.parse_args()
    
    if args.out_dir:
        os.makedirs(args.out_dir, exist_ok=True)
        banner_path = os.path.join(args.out_dir, os.path.basename(args.banner))
        icon_path = os.path.join(args.out_dir, os.path.basename(args.icon))
        obj_path = os.path.join(args.out_dir, os.path.basename(args.obj))
        mtl_path = os.path.join(args.out_dir, os.path.basename(args.mtl))
    else:
        banner_path = args.banner
        icon_path = args.icon
        obj_path = args.obj
        mtl_path = args.mtl
    
    print("=== Generador de Recursos osu! 3DS ===")
    create_banner_image(banner_path)
    create_icon_image(icon_path)
    create_3d_mesh(obj_path, mtl_path)
    print("=== Proceso completado exitosamente ===")


if __name__ == "__main__":
    main()
