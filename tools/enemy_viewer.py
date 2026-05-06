import pygame
import json
import os
import sys
import re

# Paths relative to the project root
ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
ENEMIES_CONFIG_DIR = os.path.join(ROOT_DIR, 'configs', 'enemies')

def load_json_with_comments(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
        # Remove C-style comments (// ...)
        content = re.sub(re.compile(r"//.*?\n"), "", content)
        return json.loads(content)

def save_json_with_comments(path, data, original_content):
    # This is a simplified saver that won't preserve all comments perfectly, 
    # but we'll try to just overwrite the values in the JSON structure.
    # For simplicity in this tool, we will overwrite the file.
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
    print(f"Saved to {os.path.basename(path)}!")

def main():
    pygame.init()
    screen = pygame.display.set_mode((800, 600))
    pygame.display.set_caption("Enemy Sphere Collider Viewer (W/S: Radius, Q/E: Scale, TAB: Next Enemy, Enter: Save)")
    clock = pygame.time.Clock()

    # Get list of enemy configs
    enemy_files = [f for f in os.listdir(ENEMIES_CONFIG_DIR) if f.endswith('.json')]
    if not enemy_files:
        print("No enemy config files found in configs/enemies/")
        sys.exit(1)

    current_idx = 0
    font = pygame.font.SysFont(None, 24)

    def load_current_enemy():
        file_path = os.path.join(ENEMIES_CONFIG_DIR, enemy_files[current_idx])
        config = load_json_with_comments(file_path)
        
        # Load image (handle different path structures)
        tex_path = config.get("texturePath", "")
        # Try absolute or relative to root
        full_tex_path = os.path.join(ROOT_DIR, tex_path)
        if not os.path.exists(full_tex_path):
            # Fallback for assets/IMG/Enemy/Enemy1.png if config says assets/Enemy/Enemy1.png
            full_tex_path = os.path.join(ROOT_DIR, 'assets', 'IMG', 'Enemy', os.path.basename(tex_path))

        try:
            img = pygame.image.load(full_tex_path).convert_alpha()
        except:
            # Placeholder if image not found
            img = pygame.Surface((100, 100))
            img.fill((255, 0, 255))
        
        return config, img, file_path

    config, original_img, current_file_path = load_current_enemy()

    running = True
    while running:
        scale = config.get("scale", 1.0)
        radius = config.get("radius", 1.0)
        # Handle cases where offset might not exist in JSON yet
        if "offset" not in config:
            config["offset"] = {"x": 0.0, "y": 0.0}
        offset = config["offset"]
        
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_TAB:
                    current_idx = (current_idx + 1) % len(enemy_files)
                    config, original_img, current_file_path = load_current_enemy()
                elif event.key == pygame.K_w:
                    config["radius"] += 0.5
                elif event.key == pygame.K_s:
                    config["radius"] = max(0.5, config["radius"] - 0.5)
                elif event.key == pygame.K_e:
                    config["scale"] += 0.01
                elif event.key == pygame.K_q:
                    config["scale"] = max(0.01, config["scale"] - 0.01)
                elif event.key == pygame.K_UP:
                    config["offset"]["y"] -= 1.0
                elif event.key == pygame.K_DOWN:
                    config["offset"]["y"] += 1.0
                elif event.key == pygame.K_LEFT:
                    config["offset"]["x"] -= 1.0
                elif event.key == pygame.K_RIGHT:
                    config["offset"]["x"] += 1.0
                elif event.key == pygame.K_RETURN or event.key == pygame.K_KP_ENTER:
                    save_json_with_comments(current_file_path, config, None)

        screen.fill((40, 40, 40))

        # Scale image
        w, h = original_img.get_size()
        scaled_img = pygame.transform.scale(original_img, (int(w * scale), int(h * scale)))
        rect = scaled_img.get_rect(center=(400, 300))
        screen.blit(scaled_img, rect.topleft)

        # Draw Center
        pygame.draw.circle(screen, (255, 0, 0), (400, 300), 2)

        # Draw Sphere Collider (Cyan)
        col_x = 400 + config["offset"]["x"]
        col_y = 300 + config["offset"]["y"]
        pygame.draw.circle(screen, (0, 255, 255), (int(col_x), int(col_y)), int(radius), 2)

        # Info
        texts = [
            f"Enemy: {enemy_files[current_idx]}",
            f"Radius (W/S): {radius:.1f}",
            f"Scale (Q/E): {scale:.2f}",
            f"Offset (Arrows): {config['offset']['x']}, {config['offset']['y']}",
            "TAB: Switch Enemy",
            "ENTER: Save to JSON"
        ]
        for i, t in enumerate(texts):
            img_text = font.render(t, True, (255, 255, 255))
            screen.blit(img_text, (10, 10 + i * 25))

        pygame.display.flip()
        clock.tick(60)

    pygame.quit()

if __name__ == "__main__":
    main()