import pygame
import json
import os
import sys

# Paths relative to the project root
ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
CONFIG_PATH = os.path.join(ROOT_DIR, 'configs', 'Statue.json')
IMAGE_PATH = os.path.join(ROOT_DIR, 'assets', 'IMG', 'Player', 'Statue.png')

def load_config():
    with open(CONFIG_PATH, 'r') as f:
        return json.load(f)

def save_config(config):
    with open(CONFIG_PATH, 'w') as f:
        json.dump(config, f, indent=4)
    print("Saved to Statue.json!")

def main():
    pygame.init()
    screen = pygame.display.set_mode((800, 600))
    pygame.display.set_caption("Collider Viewer (Arrows: Offset, W/A/S/D: Size, Enter: Save)")
    clock = pygame.time.Clock()

    config = load_config()
    scale = config.get("scale", 1.0)
    
    # Load and scale image
    try:
        original_img = pygame.image.load(IMAGE_PATH).convert_alpha()
    except Exception as e:
        print("Failed to load image:", e)
        sys.exit(1)
        
    img_rect = original_img.get_rect()
    scaled_img = pygame.transform.scale(original_img, (int(img_rect.width * scale), int(img_rect.height * scale)))
    
    center_x, center_y = 400, 300

    font = pygame.font.SysFont(None, 24)

    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_UP:
                    config["boxOffset"]["y"] -= 1.0
                elif event.key == pygame.K_DOWN:
                    config["boxOffset"]["y"] += 1.0
                elif event.key == pygame.K_LEFT:
                    config["boxOffset"]["x"] -= 1.0
                elif event.key == pygame.K_RIGHT:
                    config["boxOffset"]["x"] += 1.0
                elif event.key == pygame.K_w:
                    config["boxSize"]["y"] += 1.0
                elif event.key == pygame.K_s:
                    config["boxSize"]["y"] -= 1.0
                elif event.key == pygame.K_d:
                    config["boxSize"]["x"] += 1.0
                elif event.key == pygame.K_a:
                    config["boxSize"]["x"] -= 1.0
                elif event.key == pygame.K_RETURN or event.key == pygame.K_KP_ENTER:
                    save_config(config)

        screen.fill((50, 50, 50))

        # Draw Image centered
        scaled_rect = scaled_img.get_rect(center=(center_x, center_y))
        screen.blit(scaled_img, scaled_rect.topleft)

        # Draw Center point
        pygame.draw.circle(screen, (255, 0, 0), (center_x, center_y), 4)

        # Draw Collider
        b_width = config["boxSize"]["x"]
        b_height = config["boxSize"]["y"]
        b_offset_x = config["boxOffset"]["x"]
        b_offset_y = config["boxOffset"]["y"]

        # Position in game: transform.position + box.offset
        col_x = center_x + b_offset_x - b_width / 2.0
        col_y = center_y + b_offset_y - b_height / 2.0
        
        collider_rect = pygame.Rect(col_x, col_y, b_width, b_height)
        pygame.draw.rect(screen, (0, 255, 0), collider_rect, 2)

        # Draw Instructions
        text1 = font.render(f"Offset (Arrows): {b_offset_x}, {b_offset_y}", True, (255, 255, 255))
        text2 = font.render(f"Size (W/A/S/D): {b_width}, {b_height}", True, (255, 255, 255))
        text3 = font.render("Press Enter to Save to Statue.json", True, (255, 255, 0))
        screen.blit(text1, (10, 10))
        screen.blit(text2, (10, 40))
        screen.blit(text3, (10, 70))

        pygame.display.flip()
        clock.tick(60)

    pygame.quit()

if __name__ == "__main__":
    main()