# FAAD2 WASM Bindings

This project provides a WebAssembly (WASM) binding for the FAAD2 AAC decoder, allowing it to be used in web applications.  

## Setup

```shell
make setup-submodules
make setup-emsdk
```

## Build

```shell
make patch-libfaad
make build
```

## Usage Example (Minimal Decoder)

TODO: needs update on unpacking decoded data

```javascript
let instance = null
let initialized = false

import Faad2Module from './faad2.js'

export async function initDecoder(ascBytes) {
  if (!instance) {
    instance = await Faad2Module()
  }

  console.debug('capabilities', instance._get_faad_capabilities())

  const ascPtr = instance._malloc(ascBytes.length)
  instance.HEAPU8.set(ascBytes, ascPtr)

  const result = instance._init_decoder(ascPtr, ascBytes.length)
  instance._free(ascPtr)

  console.debug('init result:', result)

  if (result < 0) throw new Error('Failed to init FAAD decoder')
  initialized = true
}

export function decodeAAC(frameBytes) {
  if (!instance || !initialized) throw new Error('Decoder not initialized')

  const inPtr = instance._malloc(frameBytes.length)
  const outPtr = instance._malloc(4096 * 4 * 2)

  instance.HEAPU8.set(frameBytes, inPtr)
  const samples = instance._decode_frame(inPtr, frameBytes.length, outPtr, 4096 * 4 * 2)

  instance._free(inPtr)

  if (samples <= 0) {
    instance._free(outPtr)
    return null
  }

  const numChannels = 2
  const numFrames = samples / numChannels

  const raw = new Float32Array(instance.HEAPU8.buffer, outPtr, samples)
  const pcmData = [new Float32Array(numFrames), new Float32Array(numFrames)]

  for (let i = 0; i < numFrames; i++) {
    pcmData[0][i] = raw[i * 2]
    pcmData[1][i] = raw[i * 2 + 1]
  }

  instance._free(outPtr)

  return pcmData
}
```

## Usage Example (Full Decoder)

```javascript
// faad2_decoder.js
import Faad2Module from './faad2_wasm.mjs'

const SAMPLE_RATE = {
  1: 8000,
  2: 16000,
  3: 22050,
  4: 32000,
  5: 44100,
  6: 48000,
  7: 64000,
  8: 88200,
  9: 96000,
}

class FAAD2Decoder {
  constructor({ output, error }) {
    this.module = null
    this.initialized = false
    this.output = output
    this.error = error
    this.inputBuffer = new Uint8Array(0)
  }

  async configure({ codec, description }) {
    const asc = new Uint8Array(description)

    try {
      if (!this.module) {
        this.module = await Faad2Module()
        console.debug('FAAD2: module loaded')
        if (this.module._get_faad_capabilities) {
          console.debug('FAAD2: capabilities', this.module._get_faad_capabilities())
        }
      }

      const ascPtr = this.module._malloc(asc.length)
      this.module.HEAPU8.set(asc, ascPtr)

      const result = this.module._init_decoder(ascPtr, asc.length)
      this.module._free(ascPtr)

      if (result < 0) {
        throw new Error('Failed to initialize FAAD2 decoder')
      }

      this.initialized = true

      console.debug(
        'FAAD2Decoder: configured',
        codec,
        Array.from(asc)
          .map(b => `0x${b.toString(16).padStart(2, '0').toUpperCase()}`)
          .join(', ')
      )
    } catch (err) {
      this.error(new DOMException(err.message, 'InvalidStateError'))
    }
  }

  async reset() {
    console.debug('FAAD2Decoder: reset')
  }

  async decode(chunk) {
    if (!this.module || !this.initialized) {
      throw new Error('Decoder not initialized')
    }

    const input = new Uint8Array(chunk.byteLength)
    chunk.copyTo(input)

    const inputLength = input.length
    const pad = 64
    const inPtr = this.module._malloc(inputLength + pad)
    this.module.HEAPU8.set(input, inPtr)
    this.module.HEAPU8.fill(0, inPtr + inputLength, inPtr + inputLength + pad)

    const maxFrames = 2048 * 2
    const maxChannels = 2
    const maxSamples = maxFrames * maxChannels
    const outputSize = maxSamples * Float32Array.BYTES_PER_ELEMENT
    const outPtr = this.module._malloc(outputSize)

    const packed = this.module._decode_frame(inPtr, input.length, outPtr, outputSize)
    this.module._free(inPtr)

    if (packed <= 0) {
      this.module._free(outPtr)
      return
    }

    const samplerateIndex = (packed >>> 28) & 0xf
    const numChannels = (packed >>> 24) & 0xf
    const samples = packed & 0xffffff
    const samplerate = SAMPLE_RATE[samplerateIndex] || 0

    const numFrames = samples / numChannels
    const planeSize = numFrames * Float32Array.BYTES_PER_ELEMENT

    const raw = new Float32Array(this.module.HEAPU8.buffer, outPtr, samples)
    const buffer = new ArrayBuffer(planeSize * numChannels)
    const left = new Float32Array(buffer, 0, numFrames)
    const right = new Float32Array(buffer, planeSize, numFrames)

    for (let i = 0; i < numFrames; i++) {
      left[i] = raw[i * 2]
      right[i] = raw[i * 2 + 1]
    }

    this.module._free(outPtr)

    const audioData = new AudioData({
      format: 'f32-planar',
      sampleRate: samplerate,
      numberOfFrames: numFrames,
      numberOfChannels: 2,
      timestamp: chunk.timestamp,
      data: buffer,
      transfer: [buffer],
    })

    this.output(audioData)
  }
}

export default FAAD2Decoder
```

## Copyrights FAAD2

```text
For FAAD2 the following license applies:

******************************************************************************
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2005 M. Bakker, Nero AG, http://www.nero.com
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** The "appropriate copyright message" mentioned in section 2c of the GPLv2
** must read: "Code from FAAD2 is copyright (c) Nero AG, www.nero.com"
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Nero AG through Mpeg4AAClicense@nero.com.
******************************************************************************


Please note that the use of this software may require the payment of
patent royalties. You need to consider this issue before you start
building derivative works. We are not warranting or indemnifying you in
any way for patent royalities! YOU ARE SOLELY RESPONSIBLE FOR YOUR OWN
ACTIONS!
```

Also see [github.com/knik0/faad2](https://github.com/knik0/faad2) and
[COPYING](https://github.com/knik0/faad2/blob/master/COPYING)
