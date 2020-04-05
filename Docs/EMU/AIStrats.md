## AI Emulation Strategies

By its nature, AI is a streaming device, that is, it is not very clear how to emulate it :P

### What we have

On the DVD side:
- LR samples from DVDs come one at a time at 32 or 48 kHz sample rate. Here immediately there is a dullness - if they come one at a time, but with different frequencies - how to give them to the mixer?
- Volume: after SRC, samples pass the Volume control. Then they get into the mixer.
- It is possible to count samples and generate interrupt when counter reach value.

From the side of AI DMA:
- Samples come in AI FIFO by 32Byte packets
- Switching the AI ​​DMA sample rate is controlled by AICR[DFR] bit (not mentioned in patent)

### Howto

DMA execution must be based on emulated Gekko ticks (TBR). Sound playback should NOT be tied to the GekkoCore - we pool the accumulated sample buffer and start the sound on the backend, when it reach limit.

That is, sound emulation is seen as follows:
- AI DMA thread feeding mixer by 32Byte chunks. DMA speed is tied to TBR (executed N Gekko ticks -> shoved another 32 bytes)
- DVD Audio working in its own thread also feeds the Mixer for 1 sample. In this case, the AIS interrupt is checked on the counter. Here you need to pre-cache ADPCM from disk, since DVD::Read from the GCM image will slow down the thing.
- Mixer simply accumulates a sample buffer (mixing AI and DVD Audio) until a critical mass is typed there (the frame length is configurable). After that, all this accumulated buffer is asynchronously shoved into the playback backend.

## Стратегии эмуляции (Rus)

По своей природе AI - потоковое устройство, то есть не очень понятно как его эмулировать)

### Что у нас есть

Со стороны DVD:
- LR-Сэмплы от DVD приходят по одному на частоте сэмплирования 32 или 48 kHz. Тут сразу возникает затуп - если они приходят по одному, но с разной частотой - как их отдавать в миксер?
- Volume: после SRC сэмплы проходят контроль Volume (громкости). После чего попадают в Миксер.
- Есть возможность считать сэмплы и сгенерировать прерывание по счетчику

Со стороны AI DMA:
- Сэмлпы приходят пачкой в AI FIFO (которое, судя по реверсу размером 32 байта)
- Переключать частоту сэмплирования AI DMA можно битом AICR[DFR]

### Как эмулировать

Выполнение DMA должно происходить с привязкой к тикам Gekko (TBR). Воспроизведение звука НЕ должно быть привязано к ядру - мы пуляем накопленный буфер сэмплов (какого размера?) и запускаем звук на бэкенде.

То есть эмуляция звука видится следующим образом:
- AI DMA работая в потоке фидит Миксер. Скорость DMA завязана на TBR (выполнили N тиков Gekko - пихнули ещё 32 байта)
- DVD Audio работая в потоке тоже фидит Миксер по 1 сэмплу. При этом проверяется прерывание AIS по счетчику. Тут нужно сделать прекэширование ADPCM с диска, так как DVD::Read побайтово из образа GCM будет тормозить (размер кэша вынесем в параметры класса)
- Миксер просто накапливает буфер сэмплов (миксуя AI и DVD Audio), пока там не наберется критическая масса (длину фрейма вынесем в параметры класса). После этого весь этот накопленный буфер асинхронно пихается в бэкенд на воспроизведение.

### Частота сэмлирования

К выбору размера буфера для проигрывания на стороне бэкенда и стратегии SRC.

- 1 секунда звука при 48000 сэмплов = 48000 * 2 (разрядность) * 2 (L/R) = 192000 байт
- 32 байта при 48000 Hz = 8 сэмплов = 0.16666(7) миллисекунд звука
- 1 секунда звука при 32000 сэмплов = 32000 * 2 * 2 = 128000 байт
- 32 байта при 32000 Hz = 0.25 миллисекунд

Так как миксер выдает всегда 48 kHz сэмплы, все расчеты будут на их основе.

Примем размер фрейма равным кадровой частоте:
- 1 кадр PAL = 40 миллисекунд ~ 240 32 байтовых блоков = 7680 байт
- 1 кадр NTSC = 33.33(3) миллисекунд ~ 200 32 байтовых блоков = 6400 байт

То есть 16 Кбайт для звукового буфера хватит с запасом.

Теперь нужно посчитать сколько нужно кэшировать блоков для SRC на частоте сэмплирования 32000 Hz.
- PAL: 160 32 байтовых блоков
- NTSC: 133 32 байтовых блоков

Таким образом в миксере будем собирать (в 32-байтовых блоках):
|Sample rate|PAL|NTSC|
|---|---|---|
|32000|160|133|
|48000|240|200|

Переключение частоты сэмплирования хотя бы одного источника инвалидирует звуковой буфер.
