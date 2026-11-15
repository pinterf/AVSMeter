BlankClip(length=100, width=640, height=360, fps=25, pixel_type="YV12", audio_rate=44100, channels=2, sample_type="16bit")
# Heavier per-frame load than video.avs: Prefetch runs it on several threads.
ScriptClip("""n = 0
for (i = 1, 150000) { n = n + i }
last""")
Prefetch(4)
