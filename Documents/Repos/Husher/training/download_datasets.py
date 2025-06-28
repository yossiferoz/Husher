#!/usr/bin/env python3
"""
Download and prepare Hebrew datasets for ח detection model training
"""

import os
import requests
import zipfile
import tarfile
from pathlib import Path
import pandas as pd
from tqdm import tqdm
import argparse

def download_file(url, filepath):
    """Download file with progress bar"""
    response = requests.get(url, stream=True)
    total_size = int(response.headers.get('content-length', 0))
    
    with open(filepath, 'wb') as file, tqdm(
        desc=filepath.name,
        total=total_size,
        unit='B',
        unit_scale=True,
        unit_divisor=1024,
    ) as pbar:
        for chunk in response.iter_content(chunk_size=1024):
            if chunk:
                file.write(chunk)
                pbar.update(len(chunk))

def download_mozilla_common_voice_he(data_dir):
    """Download Mozilla Common Voice Hebrew dataset"""
    print("📥 Downloading Mozilla Common Voice Hebrew...")
    
    # Note: Actual download requires Mozilla account and agreement
    # This is a template - users need to manually download from:
    # https://commonvoice.mozilla.org/he/datasets
    
    print("⚠️  Manual download required:")
    print("1. Go to https://commonvoice.mozilla.org/he/datasets")
    print("2. Create account and agree to terms")
    print("3. Download Hebrew dataset")
    print(f"4. Extract to {data_dir}/mozilla_cv_he/")
    
    return data_dir / "mozilla_cv_he"

def download_openslr_hebrew(data_dir):
    """Download OpenSLR Hebrew speech corpus"""
    print("📥 Downloading OpenSLR Hebrew Speech Corpus...")
    
    openslr_dir = data_dir / "openslr_he"
    openslr_dir.mkdir(exist_ok=True)
    
    # OpenSLR 61: Hebrew speech corpus
    urls = [
        "https://www.openslr.org/resources/61/he_IL.tar.gz"
    ]
    
    for url in urls:
        filename = openslr_dir / url.split('/')[-1]
        if not filename.exists():
            print(f"Downloading {url}...")
            download_file(url, filename)
            
            # Extract if tar.gz
            if filename.suffix == '.gz':
                print(f"Extracting {filename}...")
                with tarfile.open(filename, 'r:gz') as tar:
                    tar.extractall(openslr_dir)
    
    return openslr_dir

def create_custom_dataset_template(data_dir):
    """Create template for custom Hebrew ח recordings"""
    print("📝 Creating custom dataset template...")
    
    custom_dir = data_dir / "custom_he_het"
    custom_dir.mkdir(exist_ok=True)
    
    # Create recording script template
    recording_script = custom_dir / "recording_script.md"
    with open(recording_script, 'w', encoding='utf-8') as f:
        f.write("""# Hebrew ח Recording Script

## Target Words (Record each 5 times):
1. חיים (Chaim) - /χaim/ - "life"
2. חם (cham) - /χam/ - "hot"  
3. חכם (chacham) - /χaχam/ - "wise"
4. חדש (chadash) - /χadaʃ/ - "new"
5. חלום (chalom) - /χalom/ - "dream"
6. אח (ach) - /aχ/ - "brother"
7. רוח (ruach) - /ruaχ/ - "spirit/wind"
8. פתח (petach) - /petaχ/ - "opening"

## Sentences with ח context:
1. חיים חכם מאוד - "Chaim is very wise"
2. היום חם ויפה - "Today is hot and beautiful"
3. חלמתי חלום מוזר - "I dreamed a strange dream"

## Recording Guidelines:
- Sample rate: 44.1kHz, 16-bit WAV
- Clean environment (minimal background noise)
- Natural speaking pace
- File naming: speaker_word_take.wav (e.g., speaker1_chaim_001.wav)
- Record 10+ speakers (diverse age/gender)

## File Structure:
custom_he_het/
├── speaker1/
│   ├── chaim_001.wav
│   ├── chaim_002.wav
│   └── ...
├── speaker2/
└── annotations.csv
""")
    
    # Create annotation template
    annotations_csv = custom_dir / "annotations.csv"
    sample_data = {
        'filename': ['speaker1_chaim_001.wav', 'speaker1_cham_001.wav'],
        'speaker_id': ['speaker1', 'speaker1'],
        'word': ['chaim', 'cham'],
        'transcription': ['חיים', 'חם'],
        'het_positions': ['0.2-0.35', '0.1-0.25'],  # Start-end times of ח sounds
        'duration': [1.2, 0.8]
    }
    pd.DataFrame(sample_data).to_csv(annotations_csv, index=False, encoding='utf-8')
    
    return custom_dir

def setup_dataset_structure(data_dir):
    """Set up directory structure for training"""
    print("📁 Setting up dataset structure...")
    
    subdirs = [
        "raw",           # Raw downloaded datasets
        "processed",     # Processed audio segments
        "features",      # Extracted features
        "annotations",   # Phoneme/word alignments
        "splits"         # Train/val/test splits
    ]
    
    for subdir in subdirs:
        (data_dir / subdir).mkdir(exist_ok=True)
    
    return data_dir

def main():
    parser = argparse.ArgumentParser(description='Download Hebrew ח detection datasets')
    parser.add_argument('--data-dir', type=str, default='./data', 
                       help='Directory to store datasets')
    parser.add_argument('--datasets', nargs='+', 
                       choices=['mozilla', 'openslr', 'custom', 'all'],
                       default=['all'], help='Which datasets to prepare')
    
    args = parser.parse_args()
    
    # Setup data directory
    data_dir = Path(args.data_dir)
    data_dir.mkdir(exist_ok=True)
    
    print(f"🎯 Setting up Hebrew ח detection datasets in {data_dir}")
    
    # Setup directory structure
    setup_dataset_structure(data_dir)
    
    # Download/prepare datasets
    datasets_to_process = args.datasets
    if 'all' in datasets_to_process:
        datasets_to_process = ['mozilla', 'openslr', 'custom']
    
    dataset_paths = {}
    
    if 'mozilla' in datasets_to_process:
        dataset_paths['mozilla'] = download_mozilla_common_voice_he(data_dir / "raw")
    
    if 'openslr' in datasets_to_process:
        dataset_paths['openslr'] = download_openslr_hebrew(data_dir / "raw")
    
    if 'custom' in datasets_to_process:
        dataset_paths['custom'] = create_custom_dataset_template(data_dir / "raw")
    
    print("✅ Dataset preparation complete!")
    print(f"📂 Data directory: {data_dir}")
    print("📋 Next steps:")
    print("1. Complete manual downloads (Mozilla Common Voice)")
    print("2. Record custom Hebrew ח samples")
    print("3. Run preprocessing: python preprocess_audio.py")
    print("4. Start training: python train_het_detector.py")
    
    return dataset_paths

if __name__ == "__main__":
    main()