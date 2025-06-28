# Hebrew ח Detection Dataset Sources

## 1. Mozilla Common Voice Hebrew
- **URL**: https://commonvoice.mozilla.org/he/datasets
- **License**: CC0 (Public Domain)
- **Content**: Crowd-sourced Hebrew speech recordings
- **Size**: ~10GB+ (varies by version)
- **Format**: MP3 files with transcriptions
- **ח Words**: Filter for words containing ח: חיים, חם, חכם, חדש, חלום, etc.

## 2. OpenSLR Hebrew Speech
- **URL**: https://www.openslr.org/61/
- **License**: Creative Commons BY-SA 4.0
- **Content**: Hebrew speech corpus with phoneme alignments
- **Size**: ~4GB
- **Format**: WAV files with forced alignments
- **ח Phonemes**: Direct phoneme-level annotations available

## 3. Custom Hebrew ח Dataset
- **Strategy**: Record native Hebrew speakers
- **Target Words**: 
  - חיים (Chaim) - /χaim/
  - חם (cham) - /χam/
  - חכם (chacham) - /χaχam/
  - חדש (chadash) - /χadaʃ/
  - חלום (chalom) - /χalom/
  - אח (ach) - /aχ/
  - רוח (ruach) - /ruaχ/
- **Speakers**: 10+ diverse speakers (age, gender, dialect)
- **Contexts**: Isolated words + sentence contexts

## Data Processing Pipeline
1. **Download and extract** audio files
2. **Phoneme alignment** using forced alignment tools
3. **ח segment extraction** (±200ms context windows)
4. **Negative sampling** (non-ח Hebrew phonemes)
5. **Feature extraction** (MFCC, spectrograms, raw audio)
6. **Train/validation/test split** (70/15/15)

## Target Model Architecture
- **Input**: 25ms audio windows (1024 samples @ 44.1kHz)
- **Features**: 13 MFCC coefficients + deltas
- **Model**: Lightweight CNN (< 1MB)
- **Output**: Binary classification (ח / not-ח) + confidence