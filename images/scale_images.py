#!/usr/bin/env python3
"""
Script to scale images to a uniform size while maintaining proportions.
Only scales down larger images, never scales up smaller ones.
"""

import os
import sys
from PIL import Image

def scale_image(input_path, output_path, target_size=(640, 640)):
    """
    Scale an image to fit within target_size while maintaining aspect ratio.
    Only scales down, never scales up.
    """
    with Image.open(input_path) as img:
        original_size = img.size
        
        # Calculate if we need to scale down
        scale_x = target_size[0] / original_size[0]
        scale_y = target_size[1] / original_size[1]
        scale = min(scale_x, scale_y)
        
        # Only scale if we're scaling down (scale < 1)
        if scale < 1:
            new_size = (int(original_size[0] * scale), int(original_size[1] * scale))
            resized_img = img.resize(new_size, Image.Resampling.LANCZOS)
            resized_img.save(output_path, optimize=True)
            print(f"Scaled {os.path.basename(input_path)}: {original_size} -> {new_size}")
            return True
        else:
            # Image is already smaller than or equal to target, don't scale
            img.save(output_path, optimize=True)
            print(f"Kept {os.path.basename(input_path)}: {original_size} (no scaling needed)")
            return False

def main():
    # Use current directory since script is now in the images directory
    images_dir = "."
    target_size = (640, 640)
    
    if not os.path.exists(images_dir):
        print(f"Error: Directory {images_dir} not found")
        sys.exit(1)
    
    print(f"Scaling images to fit within {target_size[0]}x{target_size[1]} pixels...")
    print("=" * 60)
    
    scaled_count = 0
    total_count = 0
    
    for filename in os.listdir(images_dir):
        if filename.lower().endswith('.png'):
            input_path = os.path.join(images_dir, filename)
            output_path = input_path  # Overwrite the original
            
            total_count += 1
            if scale_image(input_path, output_path, target_size):
                scaled_count += 1
    
    print("=" * 60)
    print(f"Processed {total_count} images")
    print(f"Scaled down {scaled_count} images")
    print(f"Left unchanged {total_count - scaled_count} images")

if __name__ == "__main__":
    main()
