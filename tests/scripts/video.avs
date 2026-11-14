BlankClip(length=100, width=640, height=360, fps=25, pixel_type="YV12", audio_rate=0)
# Per-frame interpreter work keeps the run above AVSMeter's 500 ms minimum runtime
# on any build/architecture (SIMD-independent).
ScriptClip("""n = 0
for (i = 1, 30000) { n = n + i }
last""")
