import wave
import math
from array import array

# ============================================================
# RUTAS
# ============================================================

# WAV original
ENTRADA = (
    r"C:\Users\joaqu\OneDrive\Documentos\GitHub\digital2"
    r"\ILI9341SPI\inicio_8bit_16k_mono.wav"
)

# WAV procesado para escucharlo primero en la computadora
SALIDA_WAV = (
    r"C:\Users\joaqu\OneDrive\Documentos\GitHub\digital2"
    r"\inicio_buzzer.wav"
)

# Archivo C generado
# NO se coloca automaticamente dentro del proyecto STM32
SALIDA_C = (
    r"C:\Users\joaqu\OneDrive\Documentos\GitHub\digital2"
    r"\musica_inicio.c"
)

# ============================================================
# CONFIGURACION
# ============================================================

FS = 16000

# Amplitud maxima respecto al centro 128.
# 115 deja un pequeño margen antes de 0 y 255.
PICO_OBJETIVO = 115.0

# Evita aplicar una ganancia exagerada a una señal muy pequeña.
GANANCIA_MAX = 4.0

# Filtros pensados para el buzzer.
FRECUENCIA_HP = 120.0
FRECUENCIA_LP = 4000.0

# Fade de 10 ms para reducir clic al iniciar/repetir.
FADE_MS = 10


# ============================================================
# FILTROS
# ============================================================

def filtro_pasabajos(datos, fc, fs):
    rc = 1.0 / (2.0 * math.pi * fc)
    dt = 1.0 / fs
    alpha = dt / (rc + dt)

    salida = array("f", [0.0] * len(datos))

    if len(datos) == 0:
        return salida

    salida[0] = datos[0]

    for i in range(1, len(datos)):
        salida[i] = (
            salida[i - 1]
            + alpha * (datos[i] - salida[i - 1])
        )

    return salida


def filtro_pasaaltos(datos, fc, fs):
    rc = 1.0 / (2.0 * math.pi * fc)
    dt = 1.0 / fs
    alpha = rc / (rc + dt)

    salida = array("f", [0.0] * len(datos))

    if len(datos) == 0:
        return salida

    salida[0] = 0.0

    for i in range(1, len(datos)):
        salida[i] = alpha * (
            salida[i - 1]
            + datos[i]
            - datos[i - 1]
        )

    return salida


# ============================================================
# LEER WAV
# ============================================================

print("Leyendo WAV...")

with wave.open(ENTRADA, "rb") as wav:
    canales = wav.getnchannels()
    bits = wav.getsampwidth() * 8
    frecuencia = wav.getframerate()
    cantidad = wav.getnframes()

    print()
    print("Informacion del archivo original:")
    print("Canales:", canales)
    print("Bits:", bits)
    print("Frecuencia:", frecuencia, "Hz")
    print("Muestras:", cantidad)
    print("Duracion:", cantidad / frecuencia, "segundos")

    if canales != 1:
        raise ValueError(
            "El archivo debe ser mono."
        )

    if bits != 8:
        raise ValueError(
            "El archivo debe ser PCM de 8 bits."
        )

    if frecuencia != FS:
        raise ValueError(
            "El archivo debe estar a 16000 Hz."
        )

    datos_originales = wav.readframes(cantidad)


# ============================================================
# CONVERTIR PCM UNSIGNED A SEÑAL CENTRADA EN CERO
# ============================================================

audio = array(
    "f",
    (float(x) - 128.0 for x in datos_originales)
)


# ============================================================
# ELIMINAR COMPONENTE DC
# ============================================================

media = sum(audio) / len(audio)

print()
print("Offset DC encontrado:", media)

for i in range(len(audio)):
    audio[i] -= media


# ============================================================
# FILTRADO
# ============================================================

print()
print("Aplicando filtro pasaaltos...")
print("Frecuencia:", FRECUENCIA_HP, "Hz")

audio = filtro_pasaaltos(
    audio,
    FRECUENCIA_HP,
    FS
)

print("Aplicando filtro pasabajos...")
print("Frecuencia:", FRECUENCIA_LP, "Hz")

audio = filtro_pasabajos(
    audio,
    FRECUENCIA_LP,
    FS
)


# ============================================================
# ANALIZAR AMPLITUD
# ============================================================

pico = max(abs(x) for x in audio)

if pico == 0:
    raise ValueError(
        "El archivo no contiene una señal de audio valida."
    )

print()
print("Pico despues del filtrado:", pico)


# ============================================================
# NORMALIZACION
# ============================================================

ganancia = PICO_OBJETIVO / pico

if ganancia > GANANCIA_MAX:
    ganancia = GANANCIA_MAX

print("Ganancia aplicada:", ganancia)


# ============================================================
# CONVERTIR NUEVAMENTE A PCM UNSIGNED DE 8 BITS
# ============================================================

resultado = bytearray()

minimo = 255
maximo = 0

for muestra in audio:
    valor = 128.0 + (muestra * ganancia)

    # Dejamos margen para evitar saturacion extrema.
    if valor > 243:
        valor = 243

    if valor < 13:
        valor = 13

    valor = int(round(valor))

    resultado.append(valor)

    if valor < minimo:
        minimo = valor

    if valor > maximo:
        maximo = valor


print()
print("Rango PCM antes del fade:")
print("Minimo:", minimo)
print("Centro: 128")
print("Maximo:", maximo)


# ============================================================
# FADE DE ENTRADA Y SALIDA
# ============================================================

fade_muestras = int(
    FS * (FADE_MS / 1000.0)
)

if fade_muestras * 2 > len(resultado):
    fade_muestras = len(resultado) // 2

for i in range(fade_muestras):
    factor = i / fade_muestras

    # Fade de entrada
    resultado[i] = int(
        128
        + (resultado[i] - 128) * factor
    )

    # Fade de salida
    j = len(resultado) - 1 - i

    resultado[j] = int(
        128
        + (resultado[j] - 128) * factor
    )


# ============================================================
# GUARDAR WAV PROCESADO
# ============================================================

print()
print("Generando WAV de prueba...")

with wave.open(SALIDA_WAV, "wb") as wav:
    wav.setnchannels(1)
    wav.setsampwidth(1)
    wav.setframerate(FS)
    wav.writeframes(resultado)


# ============================================================
# GENERAR musica_inicio.c
# ============================================================

print("Generando musica_inicio.c...")

with open(SALIDA_C, "w", encoding="utf-8") as f:
    f.write('#include "musica_inicio.h"\n\n')

    f.write(
        "const uint8_t musicaInicio[] = {\n"
    )

    # 16 muestras por linea
    for i in range(0, len(resultado), 16):
        bloque = resultado[i:i + 16]

        f.write("    ")

        f.write(
            ", ".join(
                f"0x{x:02X}"
                for x in bloque
            )
        )

        f.write(",\n")

    f.write("};\n\n")

    f.write(
        "const uint32_t musicaInicioTamano = "
        "sizeof(musicaInicio);\n"
    )


# ============================================================
# RESULTADOS
# ============================================================

print()
print("========================================")
print("PROCESO TERMINADO")
print("========================================")

print()
print("Muestras:", len(resultado))
print("Frecuencia:", FS, "Hz")
print(
    "Duracion:",
    len(resultado) / FS,
    "segundos"
)

print()
print("WAV procesado:")
print(SALIDA_WAV)

print()
print("Archivo C generado:")
print(SALIDA_C)

print()
print(
    "Escucha primero inicio_buzzer.wav "
    "antes de copiar musica_inicio.c al proyecto."
)