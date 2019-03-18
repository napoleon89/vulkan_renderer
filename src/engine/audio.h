#ifndef AUDIO_H
#define AUDIO_H

#include <soloud/soloud.h>
#include <soloud/soloud_wav.h>
#include <soloud/soloud_thread.h>

struct SoundEffect {
	SoLoud::Wav *wav;
	u8 *mem;
	u64 size;
	
	void loadFromMem(u8 *memory, u64 memory_size) {
		mem = memory;
		size = memory_size;
		wav = new SoLoud::Wav();
		wav->loadMem(mem, size, false, false);
		printf("");
	}
};

struct AudioEngine {
	SoLoud::Soloud core;
	
	virtual void init();
	virtual void playSound(SoundEffect *se);
	virtual void uninit();
};

#endif // AUDIO_H