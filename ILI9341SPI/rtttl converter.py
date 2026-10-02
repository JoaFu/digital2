# pip install librosa numpy
import numpy as np
import librosa

NOTE_NAMES = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B']

def freq_a_nota(f):
    if f is None or f <= 0 or np.isnan(f):
        return None
    midi = int(round(69 + 12 * np.log2(f / 440.0)))
    name = NOTE_NAMES[midi % 12]
    octave = midi // 12 - 1
    return f"{name}{octave}"

def cuantizar_duracion(ms, bpm):
    # En RTTTL: 1=redonda, 2=blanca, 4=negra, 8=corchea, 16=semicorchea, 32=fusa
    d = 240000.0 / (bpm * ms)
    permitidos = [1, 2, 4, 8, 16, 32]
    return min(permitidos, key=lambda x: abs(x - d))

def wav_a_rtttl(path, titulo="Tono", bpm=160, fmin="C4", fmax="C7"):
    y, sr = librosa.load(path, sr=None, mono=True)

    f0, voiced, _ = librosa.pyin(
        y,
        fmin=librosa.note_to_hz(fmin),
        fmax=librosa.note_to_hz(fmax),
        sr=sr,
        frame_length=2048
    )

    times = librosa.times_like(f0, sr=sr)
    frame_dt = np.median(np.diff(times)) if len(times) > 1 else 0.01

    eventos = []
    for f, v in zip(f0, voiced):
        nota = freq_a_nota(f) if v else None
        if eventos and eventos[-1][0] == nota:
            eventos[-1][1] += 1
        else:
            eventos.append([nota, 1])

    partes = []
    for nota, frames in eventos:
        dur_s = frames * frame_dt
        if dur_s < 0.06:   # ignora ruidos muy cortos
            continue

        d = cuantizar_duracion(dur_s * 1000, bpm)

        if nota is None:
            partes.append(f"{d}p")      # p = pausa
        else:
            partes.append(f"{d}{nota}") # ejemplo: 8E6, 16C#5

    titulo = titulo.replace(" ", "_").replace(":", "")
    return f"{titulo}:d=4,o=5,b={bpm}:{','.join(partes)}"

if __name__ == "__main__":
    rtttl = wav_a_rtttl("start_music.wav", titulo="Galaga_Start", bpm=160)
    print(rtttl)
    with open("galaga_start.rtttl", "w") as f:
        f.write(rtttl)