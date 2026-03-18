import struct

def generate_gltf_bin():
    # Dane wejściowe
    # 3x uint32 (indeksy: 0, 1, 2)
    indices = [0, 1, 2]
    
    # 3x vec3 float (pozycje)
    positions = [
        -0.5, -0.5, 0.0,
         0.5, -0.5, 0.0,
         0.0,  0.5, 0.0
    ]
    
    # 3x vec3 float (normalne)
    normals = [
        0.0, 0.0, 1.0,
        0.0, 0.0, 1.0,
        0.0, 0.0, 1.0
    ]

    with open("triangle.bin", "wb") as f:
        # 1. Pakujemy indeksy (I = unsigned int, 32-bit)
        f.write(struct.pack('3I', *indices))
        
        # 2. Pakujemy pozycje (f = float, 32-bit)
        f.write(struct.pack('9f', *positions))
        
        # 3. Pakujemy normalne (f = float, 32-bit)
        f.write(struct.pack('9f', *normals))

    print("Succefully generated triangle.bin (64 bytes)")

if __name__ == "__main__":
    generate_gltf_bin()

