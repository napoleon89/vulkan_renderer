void AudioEngine::init() {
	core.init();
}

void AudioEngine::playSound(SoundEffect *se) {
	core.play(*se->wav);
}

void AudioEngine::uninit() {
	core.deinit();
}