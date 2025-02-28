from PIL import Image, ImageDraw

# Function to draw a single dice face
def draw_dice_face(draw, top_left, size, dots, diagonal=False):
    x, y = top_left
    radius = size // 12  # The size of the dots (scaled to dice face size)
    
    # Define dot positions based on diagonal layout or standard layout
    if diagonal:
        # Positions for diagonal layout (used for dice faces 2 and 3)
        centers = [
            (x + size // 4, y + size // 4),  # Top-left corner
            (x + 3 * size // 4, y + 3 * size // 4),  # Bottom-right corner
            (x + size // 2, y + size // 2)  # Center position (for 3 dots)
        ]
    else:
        # Positions for standard layout (used for other dice faces)
        centers = [
            (x + size // 4, y + size // 4),  # Top-left corner
            (x + 3 * size // 4, y + size // 4),  # Top-right corner
            (x + size // 4, y + 3 * size // 4),  # Bottom-left corner
            (x + 3 * size // 4, y + 3 * size // 4),  # Bottom-right corner
            (x + size // 2, y + size // 2),  # Center position
            (x + size // 4, y + size // 2),  # Middle-left
            (x + 3 * size // 4, y + size // 2),  # Middle-right
        ]

    # Draw dots for the specified indices in the "dots" list
    for i in dots:
        if 1 <= i <= len(centers):  # Ensure the index is valid
            cx, cy = centers[i - 1]
            draw.ellipse((cx - radius, cy - radius, cx + radius, cy + radius), fill="black")
        else:
            print(f"ERROR: Index {i} in dots exceeds valid range.")

# Create the texture image: 192x32 pixels (6 dice faces of 32x32 each)
dice_size = 256  # Size of each dice face
texture_width, texture_height = 6 * dice_size, dice_size
background_color = (255, 228, 196)  # Cream background color

# Initialize a blank image and drawing context
image = Image.new("RGB", (texture_width, texture_height), background_color)
draw = ImageDraw.Draw(image)

# Define the dot patterns for each dice face (1 to 6)
dice_faces = [
    [5],                      # Face 1 (single dot at the center)
    [1, 2],                   # Face 2 (diagonal dots - top-left, bottom-right)
    [1, 3, 2],                # Face 3 (diagonal dots - top-left, bottom-right, center)
    [1, 2, 3, 4],             # Face 4 (four dots - all corners)
    [1, 2, 5, 3, 4],          # Face 5 (five dots - all corners + center)
    [1, 2, 6, 3, 4, 7],       # Face 6 (six dots - all corners + middle left/right)
]

# Draw each dice face, setting "diagonal=True" for faces 2 and 3
for i, dots in enumerate(dice_faces):
    diagonal = i + 1 in {2, 3}  # Faces 2 and 3 use diagonal dot layout
    draw_dice_face(draw, (i * dice_size, 0), dice_size, dots, diagonal=diagonal)

# Save the generated texture as a PNG file
image.save("dice_texture.png")
print("The texture has been saved as 'dice_texture.png'.")

