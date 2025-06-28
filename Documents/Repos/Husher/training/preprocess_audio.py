#!/usr/bin/env python3
"""
Preprocess Hebrew audio datasets for ח detection model training
"""

import os
import librosa
import numpy as np
import pandas as pd
from pathlib import Path
import soundfile as sf
from tqdm import tqdm
import argparse
import json
import re
from typing import List, Tuple, Dict

class HebrewAudioPreprocessor:
    def __init__(self, sample_rate=44100, window_size=1024, hop_length=512):
        self.sample_rate = sample_rate
        self.window_size = window_size
        self.hop_length = hop_length
        
        # Hebrew ח-containing words for filtering
        self.het_words = [
            'חיים', 'חם', 'חכם', 'חדש', 'חלום', 'אח', 'רוח', 'פתח',
            'חבר', 'חג', 'חוק', 'חשב', 'חזק', 'חיל', 'חסר', 'חפש'
        ]
        
    def extract_audio_features(self, audio: np.ndarray) -> Dict:
        """Extract features from audio segment"""
        features = {}
        
        # MFCC features
        mfcc = librosa.feature.mfcc(y=audio, sr=self.sample_rate, n_mfcc=13,
                                   hop_length=self.hop_length)
        features['mfcc'] = mfcc.T  # Transpose to (time, features)
        
        # Delta and delta-delta features
        mfcc_delta = librosa.feature.delta(mfcc)
        mfcc_delta2 = librosa.feature.delta(mfcc, order=2)
        features['mfcc_delta'] = mfcc_delta.T
        features['mfcc_delta2'] = mfcc_delta2.T
        
        # Spectral features
        spectral_centroids = librosa.feature.spectral_centroid(y=audio, sr=self.sample_rate,
                                                              hop_length=self.hop_length)
        features['spectral_centroid'] = spectral_centroids.T
        
        # Zero crossing rate
        zcr = librosa.feature.zero_crossing_rate(audio, hop_length=self.hop_length)
        features['zcr'] = zcr.T
        
        # Energy
        energy = np.sum(librosa.stft(audio, hop_length=self.hop_length) ** 2, axis=0)
        features['energy'] = energy.reshape(-1, 1)
        
        return features
    
    def segment_audio_for_het(self, audio: np.ndarray, transcript: str, 
                             duration: float) -> List[Tuple[np.ndarray, bool]]:
        """Segment audio and label ח sounds"""
        segments = []
        segment_length = int(0.025 * self.sample_rate)  # 25ms windows
        overlap = int(0.010 * self.sample_rate)  # 10ms overlap
        
        # Check if transcript contains ח
        contains_het = any(word in transcript for word in self.het_words) or 'ח' in transcript
        
        # Create sliding windows
        for start in range(0, len(audio) - segment_length, segment_length - overlap):
            end = start + segment_length
            segment = audio[start:end]
            
            # Pad if necessary
            if len(segment) < segment_length:
                segment = np.pad(segment, (0, segment_length - len(segment)))
            
            # For now, use simple heuristic: if contains ח and has fricative-like properties
            is_het = contains_het and self._is_fricative_like(segment)
            
            segments.append((segment, is_het))
        
        return segments
    
    def _is_fricative_like(self, segment: np.ndarray) -> bool:
        """Simple heuristic to identify fricative-like sounds"""
        # Calculate energy
        energy = np.mean(segment ** 2)
        
        # Calculate zero crossing rate
        zcr = librosa.feature.zero_crossing_rate(segment)[0, 0]
        
        # Fricatives typically have:
        # - Moderate energy (not silence, not too loud)
        # - High zero crossing rate (noisy characteristic)
        
        return 0.001 < energy < 0.1 and zcr > 0.05
    
    def process_mozilla_common_voice(self, data_dir: Path) -> List[Dict]:
        """Process Mozilla Common Voice Hebrew dataset"""
        print("🔄 Processing Mozilla Common Voice Hebrew...")
        
        metadata_file = data_dir / "train.tsv"
        if not metadata_file.exists():
            print(f"⚠️  Metadata file not found: {metadata_file}")
            return []
        
        # Load metadata
        df = pd.read_csv(metadata_file, sep='\t')
        
        # Filter for ח-containing sentences
        het_sentences = df[df['sentence'].str.contains('ח', na=False)]
        print(f"Found {len(het_sentences)} sentences containing ח")
        
        samples = []
        for _, row in tqdm(het_sentences.iterrows(), total=len(het_sentences)):
            audio_path = data_dir / "clips" / row['path']
            if audio_path.exists():
                try:
                    audio, _ = librosa.load(audio_path, sr=self.sample_rate)
                    duration = len(audio) / self.sample_rate
                    
                    # Segment audio
                    segments = self.segment_audio_for_het(audio, row['sentence'], duration)
                    
                    for i, (segment, is_het) in enumerate(segments):
                        features = self.extract_audio_features(segment)
                        
                        samples.append({
                            'source': 'mozilla_cv',
                            'original_file': str(audio_path),
                            'segment_id': i,
                            'is_het': is_het,
                            'transcript': row['sentence'],
                            'features': features,
                            'audio': segment
                        })
                        
                except Exception as e:
                    print(f"Error processing {audio_path}: {e}")
        
        return samples
    
    def process_openslr_hebrew(self, data_dir: Path) -> List[Dict]:
        """Process OpenSLR Hebrew dataset"""
        print("🔄 Processing OpenSLR Hebrew...")
        
        samples = []
        audio_files = list(data_dir.glob("**/*.wav"))
        
        for audio_path in tqdm(audio_files):
            try:
                audio, _ = librosa.load(audio_path, sr=self.sample_rate)
                duration = len(audio) / self.sample_rate
                
                # For OpenSLR, we assume phoneme alignments exist
                # This is a simplified version - real implementation would use alignments
                transcript = audio_path.stem  # Use filename as transcript placeholder
                
                segments = self.segment_audio_for_het(audio, transcript, duration)
                
                for i, (segment, is_het) in enumerate(segments):
                    features = self.extract_audio_features(segment)
                    
                    samples.append({
                        'source': 'openslr',
                        'original_file': str(audio_path),
                        'segment_id': i,
                        'is_het': is_het,
                        'transcript': transcript,
                        'features': features,
                        'audio': segment
                    })
                    
            except Exception as e:
                print(f"Error processing {audio_path}: {e}")
        
        return samples
    
    def process_custom_dataset(self, data_dir: Path) -> List[Dict]:
        """Process custom Hebrew ח recordings"""
        print("🔄 Processing custom Hebrew ח dataset...")
        
        annotations_file = data_dir / "annotations.csv"
        if not annotations_file.exists():
            print(f"⚠️  Annotations file not found: {annotations_file}")
            return []
        
        df = pd.read_csv(annotations_file)
        samples = []
        
        for _, row in tqdm(df.iterrows(), total=len(df)):
            audio_path = data_dir / row['filename']
            if audio_path.exists():
                try:
                    audio, _ = librosa.load(audio_path, sr=self.sample_rate)
                    duration = len(audio) / self.sample_rate
                    
                    # Parse ח positions if available
                    het_positions = []
                    if pd.notna(row['het_positions']):
                        for pos_str in row['het_positions'].split(','):
                            start, end = map(float, pos_str.strip().split('-'))
                            het_positions.append((start, end))
                    
                    # Create segments with precise ח labeling
                    segments = self._segment_with_annotations(audio, het_positions)
                    
                    for i, (segment, is_het) in enumerate(segments):
                        features = self.extract_audio_features(segment)
                        
                        samples.append({
                            'source': 'custom',
                            'original_file': str(audio_path),
                            'segment_id': i,
                            'is_het': is_het,
                            'transcript': row['transcription'],
                            'word': row['word'],
                            'speaker_id': row['speaker_id'],
                            'features': features,
                            'audio': segment
                        })
                        
                except Exception as e:
                    print(f"Error processing {audio_path}: {e}")
        
        return samples
    
    def _segment_with_annotations(self, audio: np.ndarray, 
                                 het_positions: List[Tuple[float, float]]) -> List[Tuple[np.ndarray, bool]]:
        """Segment audio with precise ח annotations"""
        segments = []
        segment_length = int(0.025 * self.sample_rate)  # 25ms
        overlap = int(0.010 * self.sample_rate)  # 10ms overlap
        
        for start in range(0, len(audio) - segment_length, segment_length - overlap):
            end = start + segment_length
            segment = audio[start:end]
            
            # Check if this segment overlaps with any ח position
            segment_start_time = start / self.sample_rate
            segment_end_time = end / self.sample_rate
            
            is_het = False
            for het_start, het_end in het_positions:
                if (segment_start_time <= het_end and segment_end_time >= het_start):
                    is_het = True
                    break
            
            # Pad if necessary
            if len(segment) < segment_length:
                segment = np.pad(segment, (0, segment_length - len(segment)))
            
            segments.append((segment, is_het))
        
        return segments
    
    def save_processed_data(self, samples: List[Dict], output_dir: Path):
        """Save processed data for training"""
        output_dir.mkdir(parents=True, exist_ok=True)
        
        # Separate features and labels
        features_list = []
        labels = []
        metadata = []
        
        for sample in samples:
            # Combine all features into single vector
            mfcc = sample['features']['mfcc']
            mfcc_delta = sample['features']['mfcc_delta']
            mfcc_delta2 = sample['features']['mfcc_delta2']
            spectral_centroid = sample['features']['spectral_centroid']
            zcr = sample['features']['zcr']
            energy = sample['features']['energy']
            
            # Average over time dimension for now (can be improved with RNNs)
            feature_vector = np.concatenate([
                np.mean(mfcc, axis=0),
                np.mean(mfcc_delta, axis=0),
                np.mean(mfcc_delta2, axis=0),
                np.mean(spectral_centroid, axis=0),
                np.mean(zcr, axis=0),
                np.mean(energy, axis=0)
            ])
            
            features_list.append(feature_vector)
            labels.append(int(sample['is_het']))
            
            # Store metadata
            metadata.append({
                'source': sample['source'],
                'original_file': sample['original_file'],
                'segment_id': sample['segment_id'],
                'transcript': sample.get('transcript', ''),
                'is_het': sample['is_het']
            })
        
        # Save as numpy arrays
        np.save(output_dir / "features.npy", np.array(features_list))
        np.save(output_dir / "labels.npy", np.array(labels))
        
        # Save metadata
        pd.DataFrame(metadata).to_csv(output_dir / "metadata.csv", index=False)
        
        print(f"💾 Saved {len(features_list)} samples to {output_dir}")
        print(f"📊 Features shape: {np.array(features_list).shape}")
        print(f"📊 ח samples: {sum(labels)}, Non-ח samples: {len(labels) - sum(labels)}")

def main():
    parser = argparse.ArgumentParser(description='Preprocess Hebrew audio for ח detection')
    parser.add_argument('--data-dir', type=str, default='./data', 
                       help='Directory containing raw datasets')
    parser.add_argument('--output-dir', type=str, default='./data/processed',
                       help='Directory to save processed data')
    parser.add_argument('--datasets', nargs='+', 
                       choices=['mozilla', 'openslr', 'custom', 'all'],
                       default=['all'], help='Which datasets to process')
    parser.add_argument('--sample-rate', type=int, default=44100,
                       help='Target sample rate')
    
    args = parser.parse_args()
    
    # Initialize preprocessor
    preprocessor = HebrewAudioPreprocessor(sample_rate=args.sample_rate)
    
    # Process datasets
    data_dir = Path(args.data_dir)
    all_samples = []
    
    datasets_to_process = args.datasets
    if 'all' in datasets_to_process:
        datasets_to_process = ['mozilla', 'openslr', 'custom']
    
    if 'mozilla' in datasets_to_process:
        mozilla_samples = preprocessor.process_mozilla_common_voice(
            data_dir / "raw" / "mozilla_cv_he"
        )
        all_samples.extend(mozilla_samples)
    
    if 'openslr' in datasets_to_process:
        openslr_samples = preprocessor.process_openslr_hebrew(
            data_dir / "raw" / "openslr_he"
        )
        all_samples.extend(openslr_samples)
    
    if 'custom' in datasets_to_process:
        custom_samples = preprocessor.process_custom_dataset(
            data_dir / "raw" / "custom_he_het"
        )
        all_samples.extend(custom_samples)
    
    # Save processed data
    output_dir = Path(args.output_dir)
    preprocessor.save_processed_data(all_samples, output_dir)
    
    print("✅ Audio preprocessing complete!")

if __name__ == "__main__":
    main()