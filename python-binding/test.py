import os
import sys

try:
    import alu_python
    import numpy as np
except ImportError as e:
    print(f"Failed to import alu_python. Make sure you ran 'pip install .' first: {e}")
    sys.exit(1)

def main():
    print("Testing Alu Python Bindings...")
    
    # Normally we'd load a real image, but since this is just a syntax/import test:
    test_image = "test.jpg"
    if not os.path.exists(test_image):
        print(f"Skipping actual image load because {test_image} does not exist.")
        return

    try:
        # 1. Load image using native Alu C++ backend
        img = alu_python.AluImage(test_image)
        print("Image loaded successfully natively!")
        
        # 2. Extract memory as zero-copy numpy array for OpenCV compatibility
        arr = img.as_numpy()
        print(f"Numpy array shape from native memory: {arr.shape}")
        
        # 3. Apply native C++ grayscale 
        img.grayscale()
        print("Applied native C++ grayscale filter.")
        
        # 4. Save via native C++ stb_image_write
        output_file = "output.jpg"
        img.save_jpg(output_file, 85)
        print(f"Saved processed image to {output_file}")
        
    except Exception as e:
        print(f"Error during native processing: {e}")

if __name__ == "__main__":
    main()
