BlankClip(length=5000, fps=25, audio_rate=48000, channels=1, sample_type="8bit").KillVideo()
# 9.6M single-sample GetAudio calls (-chunk=1) keep the run above the 500 ms minimum runtime
# without depending on SIMD.
