#version 450
#extension GL_KHR_shader_subgroup_quad : enable

layout(location = 0) out vec4 outColor;
layout(binding = 0) uniform sampler2D myTexture;

// Push constant, żeby oszukać kompilator przy indeksowaniu
layout(push_constant) uniform Keys {
    int baseIndex; 
} c;

void main() {
    // 1 & 2. Sample tekstury z nieliniowymi współrzędnymi
    vec2 uv = gl_FragCoord.xy * 0.01;
    vec4 sampledColor = texture(myTexture, uv);

    // Rezerwacja dużej tablicy (min. 16 elementów), żeby uniknąć small array optimization
    vec4 myLargeArray[16];
    for(int i = 0; i < 16; i++) {
        myLargeArray[i] = vec4(0.0);
    }

    // 3. Skomplikowane indeksowanie (Anti-copy propagation)
    // Mieszamy push constant i dane z tekstury, żeby kompilator zzgłupiał
    int complexIndex = (c.baseIndex + int(sampledColor.g * 5.0)) % 16;
    
    // Zapis do tablicy
    myLargeArray[complexIndex] = sampledColor;

    // 4. Odczyt z tablicy przy użyciu równie zakręconego indeksu
    int complexReadIndex = (c.baseIndex + int(sampledColor.g * 5.0)) % 16;
    vec4 loadedData = myLargeArray[complexReadIndex];

    // 5. Test przez Quad Broadcast (weryfikacja wątków pomocniczych)
    // Rozsyłamy dane z wątku ID 0 w obrębie quada do pozostałych trzech
    vec4 broadcastedData = subgroupQuadBroadcast(loadedData, 0);

    // Jeśli dane przetrwały i są poprawne, świecimy na zielono, jak nie - na czerwono
    if (distance(loadedData, broadcastedData) < 0.001) {
        outColor = vec4(0.0, 1.0, 0.0, 1.0); // Sukces
    } else {
        outColor = vec4(1.0, 0.0, 0.0, 1.0); // Fail (błąd sterownika!)
    }
}

