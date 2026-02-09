# btc-cw-node

A Bitcoin transaction relay that transmits and receives signed transactions over Morse code (CW) audio. Designed for off-grid, low-bandwidth broadcasting using nothing more than a speaker and microphone — or an HF radio.

## How It Works

```
                        TRANSMIT PATH
  raw_tx_hex ──> validate ──> Base43 encode ──> frame (KKK + CRC + AR)
       ──> Morse timing ──> 750 Hz audio out

                        RECEIVE PATH
  mic audio ──> Goertzel detect ──> Morse decode ──> deframe (CRC verify)
       ──> Base43 decode ──> validate ──> raw_tx_hex

                        BROADCAST
  raw_tx_hex ──> mempool.space API  or  Bitcoin Core RPC ──> txid
```

A signed Bitcoin transaction is Base43-encoded (alphabet chosen to minimise Morse air-time), wrapped in a protocol frame with CRC-32 integrity check, converted to Morse code, and played as a 750 Hz audio tone. On the receive side, a Goertzel single-frequency detector recovers the tone, a Morse decoder reconstructs the text, the frame is verified, and the original transaction bytes are recovered and validated.

## Building

### Dependencies

| Library | Purpose |
|---------|---------|
| [PortAudio](http://www.portaudio.com/) | Cross-platform audio I/O |
| [FFTW3](http://www.fftw.org/) | Fast Fourier Transform (reserved for future use) |
| [libcurl](https://curl.se/libcurl/) | HTTP client for network broadcast |
| [libsecp256k1](https://github.com/bitcoin-core/secp256k1) | ECDSA signature validation (bundled as submodule) |

On Debian/Ubuntu:

```bash
sudo apt install libportaudio2 portaudio19-dev libfftw3-dev libcurl4-openssl-dev
```

On Arch Linux:

```bash
sudo pacman -S portaudio fftw curl
```

On macOS (Homebrew):

```bash
brew install portaudio fftw curl
```

### Compile

```bash
git clone --recursive <repo-url>
cd btc-cw-node
cmake -B build
cmake --build build
```

The binary is placed at `build/btc-cw-node`.

### Optional: RTL-SDR Support

To enable reception via RTL-SDR hardware:

```bash
sudo apt install librtlsdr-dev        # Debian/Ubuntu
cmake -B build -DBTCCW_ENABLE_SDR=ON
cmake --build build
```

## Usage

```
btc-cw-node v1.0.0

Usage:
  btc-cw-node tx <raw_hex>        Validate, encode, and transmit a TX via audio
  btc-cw-node listen <seconds>    Capture audio from mic and decode
  btc-cw-node loopback <hex>      Full acoustic roundtrip test
  btc-cw-node broadcast <hex>     Broadcast a raw TX to the Bitcoin network
  btc-cw-node devices             List available audio devices
```

### Transmit a Transaction

```bash
btc-cw-node tx 0200000001aabbccdd...
```

Validates the transaction (must be properly signed), encodes it, and plays the Morse audio through the default output device.

### Listen and Decode

```bash
btc-cw-node listen 30
```

Captures 30 seconds of audio from the default input device, runs the full decode pipeline (Goertzel detection, Morse decoding, deframing, Base43 decoding, transaction validation), and prints the recovered transaction hex or a staged error message.

### Loopback Test

```bash
btc-cw-node loopback 0200000001aabbccdd...
```

Full acoustic roundtrip: encodes and plays the transaction, captures the audio, decodes it, and compares the result to the original input. Reports PASS if the roundtrip matches, MISMATCH otherwise. Useful for verifying the entire pipeline end-to-end.

### Broadcast

```bash
btc-cw-node broadcast 0200000001aabbccdd...
```

Validates the transaction and submits it to the Bitcoin network via the mempool.space API. Returns the transaction ID on success.

### List Audio Devices

```bash
btc-cw-node devices
```

Prints all detected audio devices with their indices and channel counts, useful for selecting a specific input or output device.

## Protocol

### Frame Format

```
KKK <Base43-payload><CRC-4> AR
```

| Field | Length | Description |
|-------|--------|-------------|
| `KKK ` | 4 chars | Preamble — Morse prosign for synchronisation |
| payload | variable | Base43-encoded transaction bytes |
| CRC | 4 chars | `encode_crc(crc32(payload))` — 4 Base43 characters |
| ` AR` | 3 chars | End-of-message prosign |

No space between the payload and the CRC. The CRC is always the last 4 characters before ` AR`.

### Base43 Encoding

Alphabet: `0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ +/.:-?`

The 43-character alphabet was chosen to include only characters that have short Morse code representations, minimising total air-time for Bitcoin transaction payloads. The encoding uses a big-integer base-conversion algorithm identical in structure to Base58, preserving leading zero bytes.

### Morse Timing

Uses ITU standard Morse code with PARIS timing:

| Element | Duration |
|---------|----------|
| Dot | 1 unit |
| Dash | 3 units |
| Intra-character gap | 1 unit |
| Inter-character gap | 3 units |
| Word gap | 7 units |

At 20 WPM, one unit = 60 ms. The tone frequency is 750 Hz, a standard CW pitch.

### Goertzel Detection

The receive side uses the Goertzel algorithm for single-frequency detection — far more efficient than a full FFT when only one frequency is of interest. Processing parameters:

| Parameter | Value |
|-----------|-------|
| Block size | 882 samples (~20 ms at 44100 Hz) |
| Bin index (k) | 15 (exactly centered on 750 Hz) |
| Auto-threshold | median magnitude x 3.0 |
| Hysteresis | OFF threshold = 70% of ON threshold |

### Decode Pipeline Stages

The receive pipeline processes audio through 5 stages with structured error reporting:

| Stage | Input | Output | Error Example |
|-------|-------|--------|---------------|
| 1. Goertzel | PCM float samples | `vector<bool>` tone stream | "no blocks to analyze" |
| 2. Morse Decode | tone booleans | text string | "no text recovered" |
| 3. Deframe | framed text | Base43 payload | "CRC mismatch" |
| 4. Base43 Decode | Base43 string | raw bytes | "invalid encoding" |
| 5. Validate | raw bytes | hex string | "transaction validation failed" |

If any stage fails, the pipeline returns immediately with the stage reached and all intermediate values populated up to that point.

## Architecture

```
btc-cw-node/
  deps/btc-cw-core/           Core library (git submodule)
    include/btccw/
      base43.hpp               Base43 codec
      checksum.hpp             CRC-32 + protocol framing
      morse.hpp                Morse encoder
      transaction.hpp          Bitcoin TX parser + validator
    src/
      base43.cpp
      checksum.cpp
      morse.cpp
      transaction.cpp
  include/                     Node application headers
    audio_io.hpp               PortAudio wrapper
    goertzel.hpp               Single-frequency tone detector
    morse_decoder.hpp          Morse-to-text decoder
    deframer.hpp               Protocol frame stripper + CRC verifier
    decode_pipeline.hpp        Full RX pipeline orchestrator
    gateway.hpp                Network broadcast (mempool.space / RPC)
    node_engine.hpp            Top-level orchestrator
    sdr_input.hpp              RTL-SDR input (optional)
  src/
    main.cpp                   CLI entry point
    audio_io.cpp
    goertzel.cpp
    morse_decoder.cpp
    deframer.cpp
    decode_pipeline.cpp
    gateway.cpp
    node_engine.cpp
    sdr_input.cpp
```

### Module Dependencies

```
main.cpp
  └── NodeEngine
        ├── AudioIO          (PortAudio)
        ├── DecodePipeline
        │     ├── GoertzelDetector
        │     ├── MorseDecoder ──> MorseEncoder::lookup()
        │     ├── Deframer ──> Checksum::crc32(), encode_crc()
        │     ├── Base43::decode()
        │     └── Transaction::validate()
        ├── Gateway          (libcurl)
        └── Core library
              ├── Base43::encode()
              ├── Checksum::frame()
              ├── MorseEncoder::encode()
              └── Transaction::validate(), hex_to_bytes()
```

## Configuration Defaults

| Parameter | Default | Notes |
|-----------|---------|-------|
| Sample rate | 44100 Hz | Standard audio rate |
| Tone frequency | 750 Hz | Standard CW pitch |
| Words per minute | 20 WPM | Unit duration = 60 ms |
| Goertzel block size | 882 samples | ~20 ms, bin-centered on 750 Hz |
| Broadcast backend | mempool.space | `https://mempool.space/api/tx` |
| RPC host | 127.0.0.1:8332 | For local Bitcoin Core |
| SDR center freq | 7.030 MHz | 40m CW band (optional) |

## Transaction Validation

Transactions are validated before transmission and after reception using libsecp256k1:

1. Hex string parsed to raw bytes
2. Structural parse: version, inputs (prevout + scriptSig), outputs (value + scriptPubKey), locktime
3. SegWit detection via marker byte (0x00) + flag byte (0x01)
4. DER signature extraction from scriptSig (P2PKH) or witness data (P2WPKH)
5. Public key extraction (compressed 33-byte or uncompressed 65-byte)
6. Validation: both signature and public key must parse correctly via libsecp256k1

This ensures only properly signed transactions are transmitted or accepted.

## License

See LICENSE file for details.
