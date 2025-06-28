# Husher Training Pipeline

Complete training system for Hebrew ח consonant detection model.

## Quick Start

```bash
# Setup training environment
cd training
pip install -r requirements.txt

# Download and prepare datasets
python download_datasets.py --data-dir ./data

# Preprocess audio data
python preprocess_audio.py --data-dir ./data --output-dir ./data/processed

# Train the model
python train_het_detector.py --data-dir ./data/processed --epochs 100
```

## Training Pipeline Overview

### 1. Data Collection (`download_datasets.py`)
- **Mozilla Common Voice Hebrew**: Crowd-sourced Hebrew speech
- **OpenSLR Hebrew Corpus**: Academic Hebrew speech dataset
- **Custom Recordings**: Targeted ח-word recordings

### 2. Audio Preprocessing (`preprocess_audio.py`)
- Segments audio into 25ms windows (1024 samples @ 44.1kHz)
- Extracts MFCC features (13 coefficients + deltas)
- Labels ח vs non-ח segments
- Creates balanced training dataset

### 3. Model Training (`train_het_detector.py`)
- **Architecture**: Lightweight MLP (40 → 64 → 32 → 16 → 2)
- **Target Size**: <1MB for real-time performance
- **Training**: Adam optimizer with early stopping
- **Export**: ONNX format for C++ integration

## Model Performance Targets

- **Accuracy**: >90% on Hebrew ח detection
- **Latency**: <10ms inference time
- **Size**: <1MB model file
- **Real-time**: 44.1kHz audio processing

## Dataset Structure

```
data/
├── raw/                    # Raw downloaded datasets
│   ├── mozilla_cv_he/     # Mozilla Common Voice Hebrew
│   ├── openslr_he/        # OpenSLR Hebrew corpus
│   └── custom_he_het/     # Custom ח recordings
├── processed/             # Preprocessed features
│   ├── features.npy       # Audio features (MFCC)
│   ├── labels.npy         # Binary labels (ח/non-ח)
│   └── metadata.csv       # Segment metadata
└── splits/                # Train/val/test splits
```

## Training Results

After training, you'll get:
- `models/het_detector.onnx` - Trained model for plugin
- `training_history.png` - Training curves
- `confusion_matrix.png` - Performance analysis
- `training_config.json` - Model configuration

## Integration with Plugin

1. Copy trained model: `cp models/het_detector.onnx ../Source/`
2. Rebuild plugin: `cd ../build && make`
3. Model automatically loaded by RealtimeInferenceEngine

## Custom Dataset Recording

For best results, record custom Hebrew ח samples:

### Target Words:
- חיים (Chaim) - "life"
- חם (cham) - "hot"  
- חכם (chacham) - "wise"
- חדש (chadash) - "new"
- חלום (chalom) - "dream"

### Recording Guidelines:
- 44.1kHz, 16-bit WAV format
- Clean environment (minimal noise)
- Multiple speakers (diverse age/gender)
- Natural speaking pace
- 5+ repetitions per word per speaker

## Advanced Training

### Hyperparameter Tuning:
```bash
python train_het_detector.py \
    --hidden-size 128 \
    --learning-rate 0.0005 \
    --epochs 200 \
    --batch-size 32
```

### Data Augmentation:
- Add noise injection
- Tempo/pitch variations
- Room impulse responses

### Model Optimization:
- Quantization for smaller models
- Pruning for faster inference
- Knowledge distillation

## Troubleshooting

**Low accuracy (<80%)**:
- Increase dataset size
- Better ח annotations
- Reduce false positives with negative sampling

**High latency (>20ms)**:
- Reduce model size
- Optimize feature extraction
- Use smaller input windows

**Poor real-time performance**:
- Check threading implementation
- Optimize ONNX Runtime settings
- Profile audio processing pipeline