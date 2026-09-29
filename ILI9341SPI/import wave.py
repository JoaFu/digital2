import wave
import os

# Archivo WAV de entrada
ARCHIVO = r"C:\Users\joaqu\OneDrive\Documentos\GitHub\digital2\ILI9341SPI\inicio_8bit_16k_mono.wav"

# Archivo C que se generará directamente dentro del proyecto STM32
SALIDA = r"C:\Users\joaqu\OneDrive\Documentos\GitHub\digital2\ILI9341SPI\Core\Src\musica_inicio.c"

print("Leyendo WAV...")

with wave.open(ARCHIVO, "rb") as wav:
    canales = wav.getnchannels()
    bits = wav.getsampwidth() * 8
    frecuencia = wav.getframerate()
    muestras = wav.getnframes()

    print("Canales:", canales)
    print("Bits:", bits)
    print("Frecuencia:", frecuencia)
    print("Muestras:", muestras)

    # Verificar que el WAV tenga exactamente el formato esperado
    if canales != 1:
        raise ValueError("El WAV debe ser mono.")

    if bits != 8:
        raise ValueError("El WAV debe ser PCM de 8 bits.")

    if frecuencia != 16000:
        raise ValueError("El WAV debe tener una frecuencia de 16000 Hz.")

    # Extraer únicamente las muestras PCM.
    # No se incluye la cabecera WAV.
    datos = wav.readframes(muestras)


# Verificar que exista Core/Src
directorio_salida = os.path.dirname(SALIDA)

if not os.path.exists(directorio_salida):
    raise FileNotFoundError(
        "No existe la carpeta de salida:\n" + directorio_salida
    )


print("Generando musica_inicio.c...")


with open(SALIDA, "w", encoding="utf-8") as f:

    f.write('#include "musica_inicio.h"\n\n')

    f.write("const uint8_t musicaInicio[] = {\n")

    # Escribir 16 muestras por línea
    for i in range(0, len(datos), 16):

        bloque = datos[i:i + 16]

        f.write("    ")

        f.write(
            ", ".join(
                f"0x{x:02X}" for x in bloque
            )
        )

        # No es problema dejar una coma después del último elemento
        f.write(",\n")

    f.write("};\n\n")

    f.write(
        "const uint32_t musicaInicioTamano = "
        "sizeof(musicaInicio);\n"
    )


print()
print("Conversion terminada correctamente.")
print("Bytes de audio:", len(datos))
print("Duracion:", len(datos) / frecuencia, "segundos")
print()
print("Archivo guardado en:")
print(SALIDA)