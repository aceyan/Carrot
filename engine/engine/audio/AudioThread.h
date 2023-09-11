//
// Created by jglrxavpok on 20/04/2021.
//

#pragma once

#include "Sound.h"
#include "SoundSource.h"
#include <memory>
#include <mutex>
#include <thread>
#include <core/ThreadSafeQueue.hpp>

namespace Carrot::Audio {
    class AudioThread {
    private:
        bool running = false;
        std::thread backingThread;
        std::mutex sourceMutex;
        std::vector<std::shared_ptr<SoundSource>> currentSources;
        ThreadSafeQueue<std::shared_ptr<SoundSource>> newSources;

        void threadCode();

    public:
        AudioThread();
        ~AudioThread();

        void registerSoundSource(std::shared_ptr<SoundSource> source);
    };
}